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



#define MORDINAL_HEIGHT_START                               2838270
#define MORDINAL_SIZE_TO_FEE_THRESHOLD_HEIGHT_START         2855400

struct inscription_history_entry
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


struct inscription_info
{
  uint64_t index = 0;
  crypto::hash img_data_hash = crypto::null_hash;
  std::string img_data;
  std::string current_metadata;
  uint64_t block_height = 0;
  uint64_t global_out_index = 0;
  std::vector<inscription_history_entry> history;


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

struct update_event
{
  uint64_t inscription_id;

  template <class Archive>
  inline void serialize(Archive& a, const unsigned int ver)
  {
    a& inscription_id;
  }
};

struct some_future_event
{
  uint64_t some_some;
  template <class Archive>
  inline void serialize(Archive& a, const unsigned int ver)
  {
    a& some_some;
  }
};

typedef boost::variant<update_event, some_future_event> inscription_event;


class inscriptions_container;

BOOST_CLASS_VERSION(inscription_history_entry, 1)
BOOST_CLASS_VERSION(inscription_info, 1)
BOOST_CLASS_VERSION(inscriptions_container, 4)

class inscriptions_container
{
  std::unordered_map<crypto::hash, uint64_t> m_data_hash_to_inscription; // img_data_hash -> index in m_inscriptions vector
  std::vector<inscription_info> m_inscriptions;
  bool m_was_fatal_error = false;
  uint64_t m_last_block_height = 0;
  bool m_need_resync = false;
  std::map<uint64_t, uint64_t> m_global_index_out_to_inscription; // global output indoex -> index in m_inscriptions vector
  std::vector<inscription_event> m_events;

  std::string m_config_path;
  uint64_t duplicates = 0;

public: 
  bool on_push_transaction(const cryptonote::transaction& tx, uint64_t block_height, const std::vector<uint64_t>& outs_indexes);
  bool on_pop_transaction(const cryptonote::transaction& tx, uint64_t block_height, const std::vector<uint64_t>& outs_indexes);
  bool set_block_height(uint64_t block_height);
  uint64_t get_block_height();
  bool init(const std::string& config_folder);
  bool deinit();
  uint64_t get_inscriptions_count();
  bool get_inscription_by_index(uint64_t index, inscription_info& oi);
  bool get_inscription_by_hash(const crypto::hash& h, inscription_info& oi);
  bool get_inscription_by_global_out_index(uint64_t index, inscription_info& oi);
  bool get_inscriptions(uint64_t start_offset, uint64_t count, std::vector<inscription_info>& ords);
  uint64_t get_events_count();
  uint64_t get_events(uint64_t start_offset, uint64_t count, std::vector<inscription_event>& ords);
  bool need_resync();
  void clear();

  template <class Archive>
  inline void serialize(Archive& a, const unsigned int ver)
  {
    if (ver < 4)
    {
      m_need_resync = true;
      return;
    }
    a& m_data_hash_to_inscription;
    a& m_inscriptions;
    a& m_was_fatal_error;
    a& m_last_block_height;
    a& m_global_index_out_to_inscription;
    a& m_events;
  }

private:
  bool is_transaction_fit_registration_fee(uint64_t inscription_size, uint64_t block, uint64_t fee);

  bool process_inscription_registration_entry(const cryptonote::transaction& tx, uint64_t block_height, const cryptonote::tx_extra_inscription_register& inscription_reg, const std::vector<uint64_t>& outs_indexes);
  bool process_inscription_update_entry(const cryptonote::transaction& tx, uint64_t block_height, const cryptonote::tx_extra_inscription_update& inscription_reg, const std::vector<uint64_t>& outs_indexes);

  bool unprocess_inscription_registration_entry(const cryptonote::transaction& tx, uint64_t block_height, const cryptonote::tx_extra_inscription_register& inscription_reg, const std::vector<uint64_t>& outs_indexes);
  bool unprocess_inscription_update_entry(const cryptonote::transaction& tx, uint64_t block_height, const cryptonote::tx_extra_inscription_update& inscription_reg, const std::vector<uint64_t>& outs_indexes);

};

