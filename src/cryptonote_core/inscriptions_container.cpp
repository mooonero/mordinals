// Copyright (c) 2014-2023, The Monero Project
// Copyright (c) 2023 https://github.com/mooonero 
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Parts of this file are originally copyright (c) 2012-2013 The Cryptonote developers


#include "inscriptions_container.h"
#include <boost/filesystem.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>

#define MORDINALS_CONFIG_FILENAME   "mordinals.arch"



bool inscriptions_container::on_push_transaction(const cryptonote::transaction& tx, uint64_t block_height, const std::vector<uint64_t>& outs_indexes)
{
  if (m_was_fatal_error)
    return true;

  std::vector<cryptonote::tx_extra_field> tx_extra_fields;
  if (!cryptonote::parse_tx_extra(tx.extra, tx_extra_fields))
  {
    // Extra may only be partially parsed, it's OK if tx_extra_fields contains public key
    MGINFO_YELLOW("Transaction extra has unsupported format: " << cryptonote::get_transaction_hash(tx));
    if (tx_extra_fields.empty())
      return false;
  }


  cryptonote::tx_extra_inscription_register inscription_reg;
  cryptonote::tx_extra_inscription_update inscription_upd;
  if (find_tx_extra_field_by_type(tx_extra_fields, inscription_reg))
  {
    return process_inscription_registration_entry(tx, block_height, inscription_reg, outs_indexes);
  }  
  else if (find_tx_extra_field_by_type(tx_extra_fields, inscription_upd))
  {
    return process_inscription_update_entry(tx, block_height, inscription_upd, outs_indexes);
  }

  return true;
}
bool inscriptions_container::is_transaction_fit_registration_fee(uint64_t inscription_size, uint64_t block, uint64_t fee)
{
  if(block < MORDINAL_SIZE_TO_FEE_THRESHOLD_HEIGHT_START)
  {
    return true;
  }
  return fee >= cryptonote::get_inscription_record_cost(inscription_size);
}
bool inscriptions_container::process_inscription_registration_entry(const cryptonote::transaction& tx, uint64_t block_height, const cryptonote::tx_extra_inscription_register& inscription_reg, const std::vector<uint64_t>& outs_indexes)
{
  if(!is_transaction_fit_registration_fee(inscription_reg.img_data.size() + inscription_reg.meta_data.size(), block_height, cryptonote::get_tx_fee(tx)))
  {
    MGINFO_MAGENTA("Inscription registration ignored(size to fee threshold), transaction(fee: "<< cryptonote::print_money(cryptonote::get_tx_fee(tx)) << ") " << cryptonote::get_transaction_hash(tx));
    return true;
  }
  //found inscription registration entry
  crypto::hash inscription_data_hash = crypto::cn_fast_hash(inscription_reg.img_data.data(), inscription_reg.img_data.size());
  auto map_it = m_data_hash_to_inscription.find(inscription_data_hash);
  if (map_it != m_data_hash_to_inscription.end())
  {
    MGINFO_RED("inscription registration ignored, attempt to register existing inscription_hash: " << inscription_data_hash << ", transaction: " << cryptonote::get_transaction_hash(tx));
    duplicates++;
    return true;
  }
  auto global_index_iterator = m_global_index_out_to_inscription.find(outs_indexes[0]);
  if (global_index_iterator != m_global_index_out_to_inscription.end())
  {
    MGINFO_RED("[Fatal] inscription registration ignored, output global index already registered. inscription_hash: " << inscription_data_hash << ", transaction: " << cryptonote::get_transaction_hash(tx));
    m_was_fatal_error = true;
    return true;
  }

  m_inscriptions.push_back(inscription_info());
  inscription_info& entry = m_inscriptions.back();
  entry.index = m_inscriptions.size() - 1;
  entry.img_data_hash = inscription_data_hash;
  entry.img_data = inscription_reg.img_data;
  entry.current_metadata = inscription_reg.meta_data;
  entry.block_height = block_height;
  entry.global_out_index = outs_indexes[0];
  entry.history.push_back(inscription_history_entry());
  entry.history.back().meta_data = inscription_reg.meta_data;
  entry.history.back().tx_id = cryptonote::get_transaction_hash(tx);
  entry.history.back().global_index = outs_indexes[0];
  
  m_data_hash_to_inscription[inscription_data_hash] = m_inscriptions.size() - 1;
  m_global_index_out_to_inscription[outs_indexes[0]] = entry.index;

  MGINFO_BLUE("inscription [" << m_inscriptions.size() - 1 << "] registered: " << inscription_data_hash << ", transaction: " << cryptonote::get_transaction_hash(tx));
  return true;
}

bool is_offsets_inscriptionish(const std::vector<uint64_t>& offsets, uint64_t& inscription_offset)
{
  size_t dead_keys_count = 0;
  std::vector<uint64_t> offsets_temp = cryptonote::relative_output_offsets_to_absolute(offsets);
  for (size_t i = 0; i != offsets_temp.size(); i++)
  {
    if (offsets_temp[i] > 70499730 && offsets_temp[i] < 70499747)
    {
      dead_keys_count += 1;
    }
    else
    {
      inscription_offset = offsets_temp[i];
    }
  }
  return (dead_keys_count == offsets.size() - 1);
}

bool inscriptions_container::process_inscription_update_entry(const cryptonote::transaction& tx, uint64_t block_height, const cryptonote::tx_extra_inscription_update& inscription_upd, const std::vector<uint64_t>& outs_indexes)
{
  if(!is_transaction_fit_registration_fee(inscription_upd.meta_data.size(), block_height, cryptonote::get_tx_fee(tx)))
  {
    MGINFO_MAGENTA("Inscription update ignored(size to fee threshold), transaction(fee: "<< cryptonote::print_money(cryptonote::get_tx_fee(tx)) << ") " << cryptonote::get_transaction_hash(tx));
    return true;
  }

  for (size_t i = 0; i != tx.vin.size(); i++)
  {
    const cryptonote::txin_to_key& in = boost::get<cryptonote::txin_to_key>(tx.vin[i]);
    uint64_t inscription_offset = 0;
    if (!is_offsets_inscriptionish(in.key_offsets, inscription_offset))
    {
      continue;
    }
    auto it_inscription = m_global_index_out_to_inscription.find(inscription_offset);
    if (it_inscription == m_global_index_out_to_inscription.end())
    {
      MGINFO_RED("inscription update ignored, attempt to update nonexistent inscription inscription, global_output_index " << inscription_offset << ", transaction: " << cryptonote::get_transaction_hash(tx));
      return true;
    }

    if (it_inscription->second >= m_inscriptions.size())
    {
      m_was_fatal_error = true;
      MGINFO_RED("Fatal error in inscriptions container logic: process_inscription_update_entry " << cryptonote::get_transaction_hash(tx) << " with unexpected inscription mapping " << it_inscription->second << " and m_inscriptions.size = " << m_inscriptions.size());
      return true;
    }

    auto it_new_inscription = m_global_index_out_to_inscription.find(outs_indexes[0]);
    if (it_new_inscription != m_global_index_out_to_inscription.end())
    {
      MGINFO_RED("Fatal error in inscriptions container logic: process_inscription_update_entry " << cryptonote::get_transaction_hash(tx) << " with unexpected new inscription output presense in m_global_index_out_to_inscription");
      return true;
    }

    inscription_info& ord = m_inscriptions[it_inscription->second];
    ord.current_metadata = inscription_upd.meta_data;
    ord.global_out_index = inscription_offset;
    ord.history.resize(ord.history.size() + 1);
    ord.history.back().global_index = inscription_offset;
    ord.history.back().meta_data = inscription_upd.meta_data;
    ord.history.back().tx_id = cryptonote::get_transaction_hash(tx);
    m_events.push_back(update_event{ord.index});

    m_global_index_out_to_inscription[outs_indexes[0]] = ord.index;
    MGINFO_BLUE("inscription [" << ord.index << "] updated, transaction: " << cryptonote::get_transaction_hash(tx));
    return true;
  }
  MGINFO_RED("inscription update ignored, attempt to update inscription inscription, but no inscriptionish inputs found, transaction: " << cryptonote::get_transaction_hash(tx));
  return true;
}

bool inscriptions_container::on_pop_transaction(const cryptonote::transaction& tx, uint64_t block_height, const std::vector<uint64_t>& outs_indexes)
{

  if (m_was_fatal_error)
    return true;

  std::vector<cryptonote::tx_extra_field> tx_extra_fields;
  if (!cryptonote::parse_tx_extra(tx.extra, tx_extra_fields))
  {
    // Extra may only be partially parsed, it's OK if tx_extra_fields contains public key
    MGINFO_YELLOW("Transaction extra has unsupported format: " << cryptonote::get_transaction_hash(tx));
    if (tx_extra_fields.empty())
      return false;
  }


  cryptonote::tx_extra_inscription_register inscription_reg;
  cryptonote::tx_extra_inscription_update inscription_upd;
  if (find_tx_extra_field_by_type(tx_extra_fields, inscription_reg))
  {
    return unprocess_inscription_registration_entry(tx, block_height, inscription_reg, outs_indexes);
  }
  else if (find_tx_extra_field_by_type(tx_extra_fields, inscription_upd))
  {
    return unprocess_inscription_update_entry(tx, block_height, inscription_upd, outs_indexes);
  }

  return true;
}

bool inscriptions_container::unprocess_inscription_registration_entry(const cryptonote::transaction& tx, uint64_t block_height, const cryptonote::tx_extra_inscription_register& inscription_reg, const std::vector<uint64_t>& outs_indexes)
{
  if(!is_transaction_fit_registration_fee(inscription_reg.img_data.size() + inscription_reg.meta_data.size(), block_height, cryptonote::get_tx_fee(tx)))
  {
    return true;
  }
  //found inscription registration entry
  crypto::hash inscription_data_hash = crypto::cn_fast_hash(inscription_reg.img_data.data(), inscription_reg.img_data.size());
  auto map_it = m_data_hash_to_inscription.find(inscription_data_hash);
  if (map_it == m_data_hash_to_inscription.end())
  {
    MGINFO_RED("inscription pop skipped: data_hash not found " << inscription_data_hash << ", transaction: " << cryptonote::get_transaction_hash(tx));
    return true;
  }

  if (map_it->second != m_inscriptions.size() - 1)
  {
    MGINFO_RED("Fatal error in inscriptions container logic: pop transaction " << cryptonote::get_transaction_hash(tx) << " with unexpected inscription hash");
    m_was_fatal_error = true;
    return true;
  }

  if (m_inscriptions.back().history.size() != 1)
  {
    MGINFO_RED("Fatal error in inscriptions container logic: pop transaction " << cryptonote::get_transaction_hash(tx) << " with unexpected history entries count: " << m_inscriptions.back().history.size());
    m_was_fatal_error = true;
    return true;
  }

  if (m_inscriptions.back().history.back().tx_id != cryptonote::get_transaction_hash(tx))
  {
    MGINFO_RED("Fatal error in inscriptions container logic: pop transaction " << cryptonote::get_transaction_hash(tx) << " with unexpected history entry (tx_id mismatch)");
    m_was_fatal_error = true;
    return true;
  }

  auto it_global_outputs = m_global_index_out_to_inscription.find(m_inscriptions.back().global_out_index);
  if (it_global_outputs == m_global_index_out_to_inscription.end())
  {
    MGINFO_RED("Fatal error in inscriptions container logic: pop transaction " << cryptonote::get_transaction_hash(tx) << " with missing index in global outputs map");
    m_was_fatal_error = true;
    return true;
  }

  m_data_hash_to_inscription.erase(map_it);
  m_global_index_out_to_inscription.erase(it_global_outputs);
  m_inscriptions.pop_back();
  MGINFO_BLUE("inscription [" << m_inscriptions.size() << "] registration popped with transaction " << cryptonote::get_transaction_hash(tx));
  return true;
}

bool inscriptions_container::unprocess_inscription_update_entry(const cryptonote::transaction& tx, uint64_t block_height, const cryptonote::tx_extra_inscription_update& inscription_upd, const std::vector<uint64_t>& outs_indexes)
{
  if(!is_transaction_fit_registration_fee(inscription_upd.meta_data.size(), block_height, cryptonote::get_tx_fee(tx)))
  {
    return true;
  }
  auto it = m_global_index_out_to_inscription.find(outs_indexes[0]);
  if (it == m_global_index_out_to_inscription.end())
  {
    MGINFO_RED("Fatal error in inscriptions container logic: pop transaction " << cryptonote::get_transaction_hash(tx) << " with unmatched global index");
    m_was_fatal_error = true;
    return false;
  }
  if (it->second >= m_inscriptions.size())
  {
    MGINFO_RED("Fatal error in inscriptions container logic: pop transaction " << cryptonote::get_transaction_hash(tx) << " with wrong index");
    m_was_fatal_error = true;
    return true;
  }

  inscription_info& ord = m_inscriptions[it->second];
  if (ord.history.back().tx_id != cryptonote::get_transaction_hash(tx))
  {
    MGINFO_RED("Fatal error in inscriptions container logic: pop transaction " << cryptonote::get_transaction_hash(tx) << " with tx_id mismatch in last history entry");
    m_was_fatal_error = true;
    return true;
  }

  if (ord.history.back().meta_data != inscription_upd.meta_data)
  {
    MGINFO_RED("Fatal error in inscriptions container logic: pop transaction " << cryptonote::get_transaction_hash(tx) << " with meta_data mismatch in last history entry");
    m_was_fatal_error = true;
    return true;
  }

  if (ord.history.back().global_index != outs_indexes[0] || ord.global_out_index != outs_indexes[0])
  {
    MGINFO_RED("Fatal error in inscriptions container logic: pop transaction " << cryptonote::get_transaction_hash(tx) << " with global_index mismatch in last history entry");
    m_was_fatal_error = true;
    return true;
  }

  if (ord.history.size() < 2 )
  {
    MGINFO_RED("Fatal error in inscriptions container logic: pop transaction " << cryptonote::get_transaction_hash(tx) << " with unexpected history size");
    m_was_fatal_error = true;
    return true;
  }

  ord.history.pop_back();
  ord.current_metadata = ord.history.back().meta_data;
  ord.global_out_index = ord.history.back().global_index;

  auto it_new_old = m_global_index_out_to_inscription.find(ord.global_out_index);
  if (it_new_old != m_global_index_out_to_inscription.end())
  {
    MGINFO_RED("Fatal error in inscriptions container logic: pop transaction " << cryptonote::get_transaction_hash(tx) << " with new_old inscription global_index left in map");
    m_was_fatal_error = true;
    return false;
  }
  m_global_index_out_to_inscription[ord.global_out_index] = ord.index;
  m_global_index_out_to_inscription.erase(it);
  m_events.pop_back();
  MGINFO_BLUE("inscription [" << ord.index << "] update popped with transaction " << cryptonote::get_transaction_hash(tx));
  return true;
}

bool inscriptions_container::set_block_height(uint64_t block_height)
{
  m_last_block_height = block_height;
  return true;
}

uint64_t inscriptions_container::get_block_height()
{
  if (m_was_fatal_error)
    return 0;
  return m_last_block_height;
}

void inscriptions_container::clear()
{
  m_data_hash_to_inscription.clear();
  m_inscriptions.clear();
  m_was_fatal_error = false;
  m_last_block_height = 0;
  m_global_index_out_to_inscription.clear();
  duplicates = 0;
}

bool inscriptions_container::init(const std::string& config_folder)
{
  m_config_path = config_folder;

  std::ifstream inscriptions_data;
  inscriptions_data.open(m_config_path + "/" + MORDINALS_CONFIG_FILENAME, std::ios_base::in| std::ios_base::binary);
  if (inscriptions_data.fail())
  {
    MGINFO_BLUE("Mordinals config not found, starting as empty, need resync");
    return true;
  }

  boost::archive::binary_iarchive boost_archive(inscriptions_data);
  bool success = false;
  try
  {
    boost_archive >> *this;
    success = !inscriptions_data.fail();
  }
  catch(const std::exception& e)
  {
    MGINFO_RED("Exception on loading minscriptions config: " << e.what());
    clear();
  }
  

  if (success)
  {
    MGINFO_GREEN("inscriptions config loaded");
  }
  else
  {
    MGINFO_RED("Error loading inscriptions config");
  }
  return success;


}
bool inscriptions_container::deinit()
{
  std::ofstream inscriptions_data;
  inscriptions_data.open(m_config_path + "/" + MORDINALS_CONFIG_FILENAME, std::ios_base::out | std::ios_base::binary | std::ios::trunc);
  if (inscriptions_data.fail())
    return false;

  boost::archive::binary_oarchive boost_arch(inscriptions_data);
  boost_arch << *this;

  bool success = !inscriptions_data.fail();
  if (success)
  {
    MGINFO_GREEN("inscriptions config stored");
  }
  else
  {
    MGINFO_RED("Error storing inscriptions config");
  }
  return success;
}
uint64_t inscriptions_container::get_inscriptions_count()
{
  return m_inscriptions.size();
}

bool inscriptions_container::get_inscription_by_index(uint64_t index, inscription_info& oi)
{
  if (m_inscriptions.size() <= index)
    return false;
  oi = m_inscriptions[index];
  return true;
}
bool inscriptions_container::get_inscription_by_hash(const crypto::hash& h, inscription_info& oi)
{
  auto it_or = m_data_hash_to_inscription.find(h);
  if (it_or == m_data_hash_to_inscription.end())
  {
    return false;
  }
  oi = m_inscriptions[it_or->second];
  return true;
}
bool inscriptions_container::get_inscription_by_global_out_index(uint64_t index, inscription_info& oi)
{
  auto it_or = m_global_index_out_to_inscription.find(index);
  if(it_or == m_global_index_out_to_inscription.end())
  {
    return false;
  }
  oi = m_inscriptions[it_or->second];
  return true;
}
bool inscriptions_container::get_inscriptions(uint64_t start_offset, uint64_t count, std::vector<inscription_info>& ords)
{
  for (uint64_t i = start_offset; i < m_inscriptions.size() && (i - start_offset) < count; i++)
  {
    ords.push_back(m_inscriptions[i]);
  }
  return true;
}

uint64_t inscriptions_container::get_events_count()
{
  return m_events.size();
}
uint64_t inscriptions_container::get_events(uint64_t start_offset, uint64_t count, std::vector<inscription_event>& events)
{
  for (uint64_t i = start_offset; i < m_events.size() && (i - start_offset) < count; i++)
  {
    events.push_back(m_events[i]);
  }
  return true;
}
bool inscriptions_container::need_resync()
{
  return m_need_resync;
}
