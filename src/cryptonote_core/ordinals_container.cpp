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

  std::vector<cryptonote::tx_extra_field> tx_extra_fields;
  if (!cryptonote::parse_tx_extra(tx.extra, tx_extra_fields))
  {
    // Extra may only be partially parsed, it's OK if tx_extra_fields contains public key
    LOG_PRINT_L0("Transaction extra has unsupported format: " << cryptonote::get_transaction_hash(tx));
    if (tx_extra_fields.empty())
      return false;
  }


  if (find_tx_extra_field_by_type(tx_extra_fields, ordinal_reg))
  {
    return true;
  }
  return false;
}


bool ordinals_container::on_push_transaction(const cryptonote::transaction& tx, uint64_t block_height)
{
  if (m_was_fatal_error)
    return true;
  cryptonote::tx_extra_ordinal_register ordinal_reg;
  if (!get_ordinal_register_entry(tx, ordinal_reg))
  {
    return true;
  }
  //found ordinal registration entry
  crypto::hash ordinal_data_hash = crypto::cn_fast_hash(ordinal_reg.img_data.data(), ordinal_reg.img_data.size());
  auto map_it = m_data_hash_to_ordinal.find(ordinal_data_hash);
  if (map_it != m_data_hash_to_ordinal.end())
  {
    MWARNING("Ordinal registration ignored, attempt to register existing ordinal_hash: " << ordinal_data_hash << " with_transaction " << cryptonote::get_transaction_hash(tx));
    return true;
  }
  
  m_ordinals.push_back(ordinal_info());
  ordinal_info& entry = m_ordinals.back();
  entry.index = m_ordinals.size() - 1;
  entry.img_data_hash = ordinal_data_hash;
  entry.img_data = ordinal_reg.img_data;
  entry.current_metadata = ordinal_reg.meta_data;
  entry.block_height = block_height;
  entry.history.push_back(ordinal_history_entry());
  entry.history.back().meta_data = ordinal_reg.meta_data;
  entry.history.back().tx_id = cryptonote::get_transaction_hash(tx);
  m_data_hash_to_ordinal[ordinal_data_hash ] = m_ordinals.size() - 1;
  MWARNING("Ordinal [" << m_ordinals.size() - 1 << "] registered: " << ordinal_data_hash << " with_transaction " << cryptonote::get_transaction_hash(tx));


  return true;
}
bool ordinals_container::on_pop_transaction(const cryptonote::transaction& tx)
{

  cryptonote::tx_extra_ordinal_register ordinal_reg;
  if (!get_ordinal_register_entry(tx, ordinal_reg))
  {
    return true;
  }
  //found ordinal registration entry
  crypto::hash ordinal_data_hash = crypto::cn_fast_hash(ordinal_reg.img_data.data(), ordinal_reg.img_data.size());
  auto map_it = m_data_hash_to_ordinal.find(ordinal_data_hash);
  if (map_it == m_data_hash_to_ordinal.end())
  {
    MWARNING("Ordinal pop skipped: data_hash not found " << ordinal_data_hash << " with_transaction " << cryptonote::get_transaction_hash(tx));
    return true;
  }

  if (map_it->second != m_ordinals.size() - 1)
  {
    MWARNING("Fatal error in ordinals container logic: pop transaction " << cryptonote::get_transaction_hash(tx) << " with unexpected ordinal hash");
    m_was_fatal_error = true;
    return true;
  }


  m_data_hash_to_ordinal.erase(map_it);
  m_ordinals.pop_back();

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

bool ordinals_container::init(const std::string& config_folder)
{
  m_config_path = config_folder;


  std::ifstream ordinals_data;
  ordinals_data.open(m_config_path + "/" + ORDINALS_CONFIG_FILENAME, std::ios_base::in| std::ios_base::binary);
  if (ordinals_data.fail())
  {
    MWARNING("Ordinals config not found, starting as empty, need resync");
    return true;
  }
  boost::archive::binary_iarchive boost_archive(ordinals_data);

  boost_archive >> *this;
  bool success = !ordinals_data.fail();
  if (success)
  {
    MWARNING("Ordinals config loaded");
  }
  else
  {
    MWARNING("Error loading ordinals config");
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
    MWARNING("Ordinals config stored");
  }
  else
  {
    MWARNING("Error storing ordinals config");
  }
  return success;
}
uint64_t ordinals_container::get_ordinals_count()
{
  return m_ordinals.size();
}

bool ordinals_container::get_ordinal_by_index(uint64_t index, ordinal_info& oi)
{
  if (m_ordinals.size() <= index)
    return false;
  oi = m_ordinals[index];
  return true;
}
bool ordinals_container::get_ordinal_by_hash(const crypto::hash& h, ordinal_info& oi)
{
  auto it_or = m_data_hash_to_ordinal.find(h);
  if (it_or == m_data_hash_to_ordinal.end())
  {
    return false;
  }
  oi = m_ordinals[it_or->second];
  return true;
}
bool ordinals_container::get_ordinals(uint64_t start_offset, uint64_t count, std::vector<ordinal_info>& ords)
{
  for (uint64_t i = start_offset; i < m_ordinals.size() && (i - start_offset) < count; i++)
  {
    ords.push_back(m_ordinals[i]);
  }
  return true;
}
