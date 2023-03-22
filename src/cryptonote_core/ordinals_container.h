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

#pragma once
#include <boost/asio/io_service.hpp>
#include <boost/function/function_fwd.hpp>
#if BOOST_VERSION >= 107400
#include <boost/serialization/library_version_type.hpp>
#endif
#include <boost/serialization/serialization.hpp>
#include <boost/serialization/version.hpp>
#include <boost/serialization/list.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/global_fun.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <atomic>
#include <functional>
#include <unordered_map>
#include <unordered_set>

#include "span.h"
#include "syncobj.h"
#include "string_tools.h"
#include "cryptonote_basic/cryptonote_basic.h"
#include "common/util.h"
#include "cryptonote_protocol/cryptonote_protocol_defs.h"
#include "cryptonote_tx_utils.h"
#include "crypto/hash.h"
#include "common/unordered_containers_boost_serialization.h"
#include "cryptonote_basic/cryptonote_boost_serialization.h"



#define ORDINAL_HEIGHT_START   2838270

struct ordinal_history_entry
{
  crypto::hash tx_id = crypto::null_hash;
  std::string meta_data;
  uint64_t global_index = 0;

  template <class Archive>
  inline void serialize(Archive& a, const unsigned int ver)
  {
    a& tx_id;
    a& meta_data;
    a& global_index;
  }

};


struct ordinal_info
{
  uint64_t index = 0;
  crypto::hash img_data_hash = crypto::null_hash;
  std::string img_data;
  std::string current_metadata;
  uint64_t block_height = 0;
  uint64_t global_out_index = 0;
  std::vector<ordinal_history_entry> history;


  template <class Archive>
  inline void serialize(Archive& a, const unsigned int ver)
  {
    a& index;
    a& img_data_hash;
    a& img_data;
    a& current_metadata;
    a& block_height;
    a& global_out_index;
    a& history;
  }

};


BOOST_CLASS_VERSION(ordinal_history_entry, 1)
BOOST_CLASS_VERSION(ordinal_info, 1)
BOOST_CLASS_VERSION(ordinals_container, 2)

class ordinals_container
{
  std::unordered_map<crypto::hash, uint64_t> m_data_hash_to_ordinal; // img_data_hash -> index in m_ordinals vector
  std::vector<ordinal_info> m_ordinals;
  bool m_was_fatal_error = false;
  uint64_t m_last_block_height = 0;
  bool m_need_resync = false;
  std::map<uint64_t, uint64_t> m_global_index_out_to_ordinal; // global output indoex -> index in m_ordinals vector

  std::string m_config_path;

public: 
  bool on_push_transaction(const cryptonote::transaction& tx, uint64_t block_height, const std::vector<uint64_t>& outs_indexes);
  bool on_pop_transaction(const cryptonote::transaction& tx, uint64_t block_height, const std::vector<uint64_t>& outs_indexes);
  bool set_block_height(uint64_t block_height);
  uint64_t get_block_height();
  bool init(const std::string& config_folder);
  bool deinit();
  uint64_t get_ordinals_count();
  bool get_ordinal_by_index(uint64_t index, ordinal_info& oi);
  bool get_ordinal_by_hash(const crypto::hash& h, ordinal_info& oi);
  bool get_ordinals(uint64_t start_offset, uint64_t count, std::vector<ordinal_info>& ords);
  bool need_resync();


  template <class Archive>
  inline void serialize(Archive& a, const unsigned int ver)
  {
    if (ver < 2)
    {
      m_need_resync = true;
    }
    a& m_data_hash_to_ordinal;
    a& m_ordinals;
    a& m_was_fatal_error;
    a& m_last_block_height;
    a& m_global_index_out_to_ordinal;
  }

private:
  bool process_ordinal_registration_entry(const cryptonote::transaction& tx, uint64_t block_height, const cryptonote::tx_extra_ordinal_register& ordinal_reg, const std::vector<uint64_t>& outs_indexes);
  bool process_ordinal_update_entry(const cryptonote::transaction& tx, uint64_t block_height, const cryptonote::tx_extra_ordinal_update& ordinal_reg, const std::vector<uint64_t>& outs_indexes);

  bool unprocess_ordinal_registration_entry(const cryptonote::transaction& tx, uint64_t block_height, const cryptonote::tx_extra_ordinal_register& ordinal_reg, const std::vector<uint64_t>& outs_indexes);
  bool unprocess_ordinal_update_entry(const cryptonote::transaction& tx, uint64_t block_height, const cryptonote::tx_extra_ordinal_update& ordinal_reg, const std::vector<uint64_t>& outs_indexes);

};

