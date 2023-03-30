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


#include "ordinals_container.h"
#include <boost/filesystem.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>

#define ORDINALS_CONFIG_FILENAME   "ordinals.arch"

bool get_ordinal_register_entry(const cryptonote::transaction& tx, cryptonote::tx_extra_ordinal_register& ordinal_reg)
{





  return false;
}

bool ordinals_container::on_push_transaction(const cryptonote::transaction& tx, uint64_t block_height, const std::vector<uint64_t>& outs_indexes)
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


  cryptonote::tx_extra_ordinal_register ordinal_reg;
  cryptonote::tx_extra_ordinal_update ordinal_upd;
  if (find_tx_extra_field_by_type(tx_extra_fields, ordinal_reg))
  {
    return process_ordinal_registration_entry(tx, block_height, ordinal_reg, outs_indexes);
  }  
  else if (find_tx_extra_field_by_type(tx_extra_fields, ordinal_upd))
  {
    return process_ordinal_update_entry(tx, block_height, ordinal_upd, outs_indexes);
  }

  return true;
}
bool ordinals_container::is_transaction_fit_registration_fee(uint64_t inscription_size, uint64_t block, uint64_t fee)
{
  if(block < MORDINAL_SIZE_TO_FEE_MODERATION_HEIGHT_START)
  {
    return true;
  }
  return fee >= cryptonote::get_inscription_registration_cost(inscription_size);
}
bool ordinals_container::process_ordinal_registration_entry(const cryptonote::transaction& tx, uint64_t block_height, const cryptonote::tx_extra_ordinal_register& ordinal_reg, const std::vector<uint64_t>& outs_indexes)
{
  if(!is_transaction_fit_registration_fee(ordinal_reg.img_data.size(), block_height, cryptonote::get_tx_fee(tx)))
  {
    MGINFO_MAGENTA("Inscription registration ignored(size to fee moderation), transaction(fee: "<< cryptonote::print_money(cryptonote::get_tx_fee(tx)) << ") " << cryptonote::get_transaction_hash(tx));
    return true;
  }
  //found ordinal registration entry
  crypto::hash ordinal_data_hash = crypto::cn_fast_hash(ordinal_reg.img_data.data(), ordinal_reg.img_data.size());
  auto map_it = m_data_hash_to_ordinal.find(ordinal_data_hash);
  if (map_it != m_data_hash_to_ordinal.end())
  {
    MGINFO_RED("Ordinal registration ignored, attempt to register existing ordinal_hash: " << ordinal_data_hash << ", transaction: " << cryptonote::get_transaction_hash(tx));
    duplicates++;
    return true;
  }
  auto global_index_iterator = m_global_index_out_to_ordinal.find(outs_indexes[0]);
  if (global_index_iterator != m_global_index_out_to_ordinal.end())
  {
    MGINFO_RED("[Fatal] Ordinal registration ignored, output global index already registered. ordinal_hash: " << ordinal_data_hash << ", transaction: " << cryptonote::get_transaction_hash(tx));
    m_was_fatal_error = true;
    return true;
  }

  m_ordinals.push_back(inscription_info());
  inscription_info& entry = m_ordinals.back();
  entry.index = m_ordinals.size() - 1;
  entry.img_data_hash = ordinal_data_hash;
  entry.img_data = ordinal_reg.img_data;
  entry.current_metadata = ordinal_reg.meta_data;
  entry.block_height = block_height;
  entry.global_out_index = outs_indexes[0];
  entry.history.push_back(inscription_history_entry());
  entry.history.back().meta_data = ordinal_reg.meta_data;
  entry.history.back().tx_id = cryptonote::get_transaction_hash(tx);
  entry.history.back().global_index = outs_indexes[0];
  
  m_data_hash_to_ordinal[ordinal_data_hash] = m_ordinals.size() - 1;
  m_global_index_out_to_ordinal[outs_indexes[0]] = entry.index;

  MGINFO_BLUE("Ordinal [" << m_ordinals.size() - 1 << "] registered: " << ordinal_data_hash << ", transaction: " << cryptonote::get_transaction_hash(tx));
  return true;
}

bool is_offsets_ordinalish(const std::vector<uint64_t>& offsets, uint64_t& inscription_offset)
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

bool ordinals_container::process_ordinal_update_entry(const cryptonote::transaction& tx, uint64_t block_height, const cryptonote::tx_extra_ordinal_update& ordinal_upd, const std::vector<uint64_t>& outs_indexes)
{
  for (size_t i = 0; i != tx.vin.size(); i++)
  {
    const cryptonote::txin_to_key& in = boost::get<cryptonote::txin_to_key>(tx.vin[i]);
    uint64_t inscription_offset = 0;
    if (!is_offsets_ordinalish(in.key_offsets, inscription_offset))
    {
      continue;
    }
    auto it_inscription = m_global_index_out_to_ordinal.find(inscription_offset);
    if (it_inscription == m_global_index_out_to_ordinal.end())
    {
      MGINFO_RED("Ordinal update ignored, attempt to update nonexistent ordinal inscription, global_output_index " << inscription_offset << ", transaction: " << cryptonote::get_transaction_hash(tx));
      return true;
    }

    if (it_inscription->second >= m_ordinals.size())
    {
      m_was_fatal_error = true;
      MGINFO_RED("Fatal error in ordinals container logic: process_ordinal_update_entry " << cryptonote::get_transaction_hash(tx) << " with unexpected inscription mapping " << it_inscription->second << " and m_ordinals.size = " << m_ordinals.size());
      return true;
    }

    auto it_new_inscription = m_global_index_out_to_ordinal.find(outs_indexes[0]);
    if (it_new_inscription != m_global_index_out_to_ordinal.end())
    {
      MGINFO_RED("Fatal error in ordinals container logic: process_ordinal_update_entry " << cryptonote::get_transaction_hash(tx) << " with unexpected new inscription output presense in m_global_index_out_to_ordinal");
      return true;
    }

    inscription_info& ord = m_ordinals[it_inscription->second];
    ord.current_metadata = ordinal_upd.meta_data;
    ord.global_out_index = inscription_offset;
    ord.history.resize(ord.history.size() + 1);
    ord.history.back().global_index = inscription_offset;
    ord.history.back().meta_data = ordinal_upd.meta_data;
    ord.history.back().tx_id = cryptonote::get_transaction_hash(tx);
    m_events.push_back(update_event{ord.index});

    m_global_index_out_to_ordinal[outs_indexes[0]] = ord.index;
    MGINFO_BLUE("Ordinal [" << ord.index << "] updated, transaction: " << cryptonote::get_transaction_hash(tx));
    return true;
  }
  MGINFO_RED("Ordinal update ignored, attempt to update ordinal inscription, but no ordinalish inputs found, transaction: " << cryptonote::get_transaction_hash(tx));
  return true;
}

bool ordinals_container::on_pop_transaction(const cryptonote::transaction& tx, uint64_t block_height, const std::vector<uint64_t>& outs_indexes)
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


  cryptonote::tx_extra_ordinal_register ordinal_reg;
  cryptonote::tx_extra_ordinal_update ordinal_upd;
  if (find_tx_extra_field_by_type(tx_extra_fields, ordinal_reg))
  {
    return unprocess_ordinal_registration_entry(tx, block_height, ordinal_reg, outs_indexes);
  }
  else if (find_tx_extra_field_by_type(tx_extra_fields, ordinal_upd))
  {
    return unprocess_ordinal_update_entry(tx, block_height, ordinal_upd, outs_indexes);
  }

  return true;
}

bool ordinals_container::unprocess_ordinal_registration_entry(const cryptonote::transaction& tx, uint64_t block_height, const cryptonote::tx_extra_ordinal_register& ordinal_reg, const std::vector<uint64_t>& outs_indexes)
{
  if(!is_transaction_fit_registration_fee(ordinal_reg.img_data.size(), block_height, cryptonote::get_tx_fee(tx)))
  {
    return true;
  }
  //found ordinal registration entry
  crypto::hash ordinal_data_hash = crypto::cn_fast_hash(ordinal_reg.img_data.data(), ordinal_reg.img_data.size());
  auto map_it = m_data_hash_to_ordinal.find(ordinal_data_hash);
  if (map_it == m_data_hash_to_ordinal.end())
  {
    MGINFO_RED("Ordinal pop skipped: data_hash not found " << ordinal_data_hash << ", transaction: " << cryptonote::get_transaction_hash(tx));
    return true;
  }

  if (map_it->second != m_ordinals.size() - 1)
  {
    MGINFO_RED("Fatal error in ordinals container logic: pop transaction " << cryptonote::get_transaction_hash(tx) << " with unexpected ordinal hash");
    m_was_fatal_error = true;
    return true;
  }

  if (m_ordinals.back().history.size() != 1)
  {
    MGINFO_RED("Fatal error in ordinals container logic: pop transaction " << cryptonote::get_transaction_hash(tx) << " with unexpected history entries count: " << m_ordinals.back().history.size());
    m_was_fatal_error = true;
    return true;
  }

  if (m_ordinals.back().history.back().tx_id != cryptonote::get_transaction_hash(tx))
  {
    MGINFO_RED("Fatal error in ordinals container logic: pop transaction " << cryptonote::get_transaction_hash(tx) << " with unexpected history entry (tx_id mismatch)");
    m_was_fatal_error = true;
    return true;
  }

  auto it_global_outputs = m_global_index_out_to_ordinal.find(m_ordinals.back().global_out_index);
  if (it_global_outputs == m_global_index_out_to_ordinal.end())
  {
    MGINFO_RED("Fatal error in ordinals container logic: pop transaction " << cryptonote::get_transaction_hash(tx) << " with missing index in global outputs map");
    m_was_fatal_error = true;
    return true;
  }

  m_data_hash_to_ordinal.erase(map_it);
  m_global_index_out_to_ordinal.erase(it_global_outputs);
  m_ordinals.pop_back();
  MGINFO_BLUE("Ordinal [" << m_ordinals.size() << "] registration popped with transaction " << cryptonote::get_transaction_hash(tx));
  return true;
}

bool ordinals_container::unprocess_ordinal_update_entry(const cryptonote::transaction& tx, uint64_t block_height, const cryptonote::tx_extra_ordinal_update& ordinal_upd, const std::vector<uint64_t>& outs_indexes)
{
  auto it = m_global_index_out_to_ordinal.find(outs_indexes[0]);
  if (it == m_global_index_out_to_ordinal.end())
  {
    MGINFO_RED("Fatal error in ordinals container logic: pop transaction " << cryptonote::get_transaction_hash(tx) << " with unmatched global index");
    m_was_fatal_error = true;
    return false;
  }
  if (it->second >= m_ordinals.size())
  {
    MGINFO_RED("Fatal error in ordinals container logic: pop transaction " << cryptonote::get_transaction_hash(tx) << " with wrong index");
    m_was_fatal_error = true;
    return true;
  }

  inscription_info& ord = m_ordinals[it->second];
  if (ord.history.back().tx_id != cryptonote::get_transaction_hash(tx))
  {
    MGINFO_RED("Fatal error in ordinals container logic: pop transaction " << cryptonote::get_transaction_hash(tx) << " with tx_id mismatch in last history entry");
    m_was_fatal_error = true;
    return true;
  }

  if (ord.history.back().meta_data != ordinal_upd.meta_data)
  {
    MGINFO_RED("Fatal error in ordinals container logic: pop transaction " << cryptonote::get_transaction_hash(tx) << " with meta_data mismatch in last history entry");
    m_was_fatal_error = true;
    return true;
  }

  if (ord.history.back().global_index != outs_indexes[0] || ord.global_out_index != outs_indexes[0])
  {
    MGINFO_RED("Fatal error in ordinals container logic: pop transaction " << cryptonote::get_transaction_hash(tx) << " with global_index mismatch in last history entry");
    m_was_fatal_error = true;
    return true;
  }

  if (ord.history.size() < 2 )
  {
    MGINFO_RED("Fatal error in ordinals container logic: pop transaction " << cryptonote::get_transaction_hash(tx) << " with unexpected history size");
    m_was_fatal_error = true;
    return true;
  }

  ord.history.pop_back();
  ord.current_metadata = ord.history.back().meta_data;
  ord.global_out_index = ord.history.back().global_index;

  auto it_new_old = m_global_index_out_to_ordinal.find(ord.global_out_index);
  if (it_new_old != m_global_index_out_to_ordinal.end())
  {
    MGINFO_RED("Fatal error in ordinals container logic: pop transaction " << cryptonote::get_transaction_hash(tx) << " with new_old inscription global_index left in map");
    m_was_fatal_error = true;
    return false;
  }
  m_global_index_out_to_ordinal[ord.global_out_index] = ord.index;
  m_global_index_out_to_ordinal.erase(it);
  m_events.pop_back();
  MGINFO_BLUE("Ordinal [" << ord.index << "] update popped with transaction " << cryptonote::get_transaction_hash(tx));
  return true;
}

bool ordinals_container::set_block_height(uint64_t block_height)
{
  m_last_block_height = block_height;
  return true;
}

uint64_t ordinals_container::get_block_height()
{
  if (m_was_fatal_error)
    return 0;
  return m_last_block_height;
}

void ordinals_container::clear()
{
  m_data_hash_to_ordinal.clear();
  m_ordinals.clear();
  m_was_fatal_error = false;
  m_last_block_height = 0;
  m_global_index_out_to_ordinal.clear();
  duplicates = 0;
}

bool ordinals_container::init(const std::string& config_folder)
{
  m_config_path = config_folder;


  std::ifstream ordinals_data;
  ordinals_data.open(m_config_path + "/" + ORDINALS_CONFIG_FILENAME, std::ios_base::in| std::ios_base::binary);
  if (ordinals_data.fail())
  {
    MGINFO_BLUE("Ordinals config not found, starting as empty, need resync");
    return true;
  }

  boost::archive::binary_iarchive boost_archive(ordinals_data);
  bool success = false;
  try
  {
    boost_archive >> *this;
    success = !ordinals_data.fail();
  }
  catch(const std::exception& e)
  {
    MGINFO_RED("Exception on loading mordinals config: " << e.what());
    clear();
  }
  

  if (success)
  {
    MGINFO_GREEN("Ordinals config loaded");
  }
  else
  {
    MGINFO_RED("Error loading ordinals config");
  }
  return success;

}
bool ordinals_container::deinit()
{
  std::ofstream ordinals_data;
  ordinals_data.open(m_config_path + "/" + ORDINALS_CONFIG_FILENAME, std::ios_base::out | std::ios_base::binary | std::ios::trunc);
  if (ordinals_data.fail())
    return false;

  boost::archive::binary_oarchive boost_arch(ordinals_data);
  boost_arch << *this;

  bool success = !ordinals_data.fail();
  if (success)
  {
    MGINFO_GREEN("Ordinals config stored");
  }
  else
  {
    MGINFO_RED("Error storing ordinals config");
  }
  return success;
}
uint64_t ordinals_container::get_ordinals_count()
{
  return m_ordinals.size();
}

bool ordinals_container::get_ordinal_by_index(uint64_t index, inscription_info& oi)
{
  if (m_ordinals.size() <= index)
    return false;
  oi = m_ordinals[index];
  return true;
}
bool ordinals_container::get_ordinal_by_hash(const crypto::hash& h, inscription_info& oi)
{
  auto it_or = m_data_hash_to_ordinal.find(h);
  if (it_or == m_data_hash_to_ordinal.end())
  {
    return false;
  }
  oi = m_ordinals[it_or->second];
  return true;
}
bool ordinals_container::get_ordinal_by_global_out_index(uint64_t index, inscription_info& oi)
{
  auto it_or = m_global_index_out_to_ordinal.find(index);
  if(it_or == m_global_index_out_to_ordinal.end())
  {
    return false;
  }
  oi = m_ordinals[it_or->second];
  return true;
}
bool ordinals_container::get_ordinals(uint64_t start_offset, uint64_t count, std::vector<inscription_info>& ords)
{
  for (uint64_t i = start_offset; i < m_ordinals.size() && (i - start_offset) < count; i++)
  {
    ords.push_back(m_ordinals[i]);
  }
  return true;
}

uint64_t ordinals_container::get_events_count()
{
  return m_events.size();
}
uint64_t ordinals_container::get_events(uint64_t start_offset, uint64_t count, std::vector<inscription_event>& events)
{
  for (uint64_t i = start_offset; i < m_events.size() && (i - start_offset) < count; i++)
  {
    events.push_back(m_events[i]);
  }
  return true;
}
bool ordinals_container::need_resync()
{
  return m_need_resync;
}
