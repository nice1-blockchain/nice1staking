#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/symbol.hpp>
#include <eosio/string.hpp>
#include <eosio/system.hpp>

using namespace eosio;


class [[eosio::contract("n1tokentonft")]] n1tokentonft : public contract {
public:
  using contract::contract;

  // n1tokentonft: Set up a new stake campaign
    //campaign - Name of the campaign in the table
    //filler - Authorized account to add rewards to the campaign
    //start - UNIX time to start the campaign
    //finish - UNIX time to end the campaign
    //timetoreward - UNIX time needed to claim rewards
    //nftaccount - Contract that manages NFT used in the campaign
    //tokenaccount - Contract that handles Tokens used in the campaign
    //entry - Number of tokens required for participation
    //return_entry - Indicates whether tokens will be returned together with the reward 
    //places - Maximum number of participants in the campaign
    //islimited - Indicates that NFT as a reward is limited
    //printondemant - Indicates that NFT rewards will be created for the campaign
    //memo_expected - Used as a match for the contract logics  

  [[eosio::action]]
  void newcampign(name campaign, name filler, uint64_t start, uint64_t finish,
                  uint64_t timetoreward, name nftaccount, name tokenaccount,
                  asset entry, bool return_entry, uint64_t places, bool islimited,
                  bool printondemand, uint64_t memo_expected)  {
    require_auth(get_self());

    datacampaign_table datacampaign(get_self(), get_self().value);
    auto existing = datacampaign.find(campaign.value);
    check(existing == datacampaign.end(), "Campaign already exists");

    check(memo_expected > 0, "memo_expected must be a valid number");

    auto existing_memo = datacampaign.get_index<"bymemo"_n>();
    auto itr = existing_memo.find(memo_expected);
    check(itr == existing_memo.end(), "Already an entry with the same memo_expected");

    time_point_sec current_time = current_time_point();
    uint64_t current_time_sec = current_time.sec_since_epoch();

    if (start < current_time.sec_since_epoch()) {
      check(false, "Date specified in 'start' has already passed and is invalid.");
    }

    if (start >= finish) {
      check(false, "Start time must be before the end time.");
    }

    if (timetoreward >= (finish - start)) {
      check(false, "Duration is greater to active time");
    }

    if (!((islimited && !printondemand) || (!islimited && printondemand))) {
      check(false, "Select exactly one, 'islimited' or 'printondemand'.");
    }

    
    if (printondemand && places != 0) {
      eosio::check(false, "Places must be 0 (unlimited) when 'printondemand' is active");
    }
    
    datacampaign.emplace(get_self(), [&](auto& row) {
      row.campaign = campaign;
      row.filler = filler;
      row.start = start;
      row.finish = finish;
      row.timetoreward = timetoreward;
      row.nftaccount = nftaccount;
      row.tokenaccount = tokenaccount;
      row.entry = entry;
      row.return_entry = return_entry;
      row.places = places;
      row.islimited = islimited;
      row.printondemand = printondemand;
      row.memo_expected = memo_expected;
    });
  }


  // Allows you to delete an entry from datacampaign.
    //campaign - The campaign you want to delete from table.
  [[eosio::action]]
  void delcampaign(name campaign) {
    require_auth(get_self());

    datacampaign_table datacampaign(get_self(), get_self().value);
    auto existing = datacampaign.find(campaign.value);

    if (existing != datacampaign.end()) {
      datacampaign.erase(existing);
    } else {
      check(false, "The campaign does not exist ");
    }
  }


  // The logic of reception of NFTs and add as reward
  [[eosio::on_notify("simpleassets::transfer")]]
  void nft_transfer_in(name from, name to, std::vector<uint64_t>& assetids, std::string memo) {
    if (to == get_self()) {
      
      uint64_t memo_value = std::stoull(memo);

      datacampaign_table datacampaign(get_self(), get_self().value); 
      auto existing_memo = datacampaign.get_index<"bymemo"_n>();
      auto itr = existing_memo.find(memo_value);

      if (itr != existing_memo.end()) {
       
        time_point_sec current_time = current_time_point();
        uint64_t current_time_sec = current_time.sec_since_epoch();

        if (itr->printondemand == 1) {
          eosio::check(false, "Printondemand does not require filling");
        }

        
        if (current_time_sec >= itr->start) {
          
          eosio::check(false, "The campaign has already started");
        }

        
        name expected_contract = itr->nftaccount;

        
        name sender_contract = get_first_receiver();

        
        if (sender_contract != expected_contract) {
          
          eosio::check(false, "Unexpected issuer contract");
        }

        
        name transfer_from = from;

        
        name entry_filler = itr->filler;

        
        if (transfer_from != entry_filler) {
          
          eosio::check(false, "Value in 'from' does not match 'filler'.");
        }

        std::string id_str;
        for (uint64_t id : assetids) {
          id_str += std::to_string(id);
        }

        uint64_t id_uint64 = std::stoull(id_str);

        rewards_table rewards(get_self(), get_self().value);

        auto campaign_entries = rewards.get_index<"bycampaign"_n>();
        auto campaign_entries_itr = campaign_entries.lower_bound(itr->campaign.value);
        auto campaign_entries_end = campaign_entries.upper_bound(itr->campaign.value);

        
        uint64_t campaign_entry_count = 0;
        for (auto it = campaign_entries_itr; it != campaign_entries_end; ++it) {
          campaign_entry_count++;
        }

        
        uint64_t places_limit = itr->places;

        
        if (campaign_entry_count >= places_limit) {
          
          eosio::check(false, "Limit of rewards for this campaign has been reached.");
        }
        rewards.emplace(get_self(), [&](auto& row) {
          row.id = id_uint64;
          row.campaign = itr->campaign;
          row.available = true;
          row.delivered = false;
        });
      } else {
        eosio::check(false, "Memo not matching a campaign");
      }
    }
  }


  // Allows deletion of a single reward if the campaign has not yet started.
    //ID - Enter specific ID of entry to be deleted.
  [[eosio::action]]
  void delreward(uint64_t id) {
    require_auth(get_self());

    rewards_table rewards(get_self(), get_self().value);

    auto existing = rewards.find(id);

    if (existing != rewards.end()) {
        name campaign = existing->campaign;

        datacampaign_table datacampaign(get_self(), get_self().value);
        auto datacampaign_entry = datacampaign.find(campaign.value);

        if (datacampaign_entry == datacampaign.end()) {
            rewards.erase(existing);
        } else {
            time_point_sec current_time = current_time_point();
            uint64_t current_time_sec = current_time.sec_since_epoch();
            if (current_time_sec >= datacampaign_entry->start) {                
                eosio::check(false, "The campaign has already started, entry cannot be deleted.");
            } else {
                rewards.erase(existing);
            }
        }
    } else {
        eosio::check(false, "Reward ID not found in rewards table");
    }
}


  // Allows you to delete all "Rewards" from same campaign
    //camptoclear - Name of the campaign you want to delete rewards
    //memo - "Confirm" for security
  [[eosio::action]]
  void clearrewards(name camptoclear, std::string memo) {
      require_auth(get_self());

      eosio::check(memo == "confirm", "Fail.");

      rewards_table rewards(get_self(), get_self().value);

      auto reward_entry = rewards.begin();
      auto reward_end = rewards.end();

      bool foundEntries = false;

      while (reward_entry != reward_end) {
          if (reward_entry->campaign == camptoclear) {       
              reward_entry = rewards.erase(reward_entry);
              foundEntries = true; 
          } else {

              reward_entry++;
          }
      }
      eosio::check(foundEntries, "No entries were found for specified campaign.");
  }


  // Logic for receiving tokens, assigning NFT and registering as a staker.
  [[eosio::on_notify("niceonetoken::transfer")]]
  void user_join(name from, name to, asset quantity, std::string memo) {
      if (to == get_self()) {
          uint64_t memo_value = std::stoull(memo);

          datacampaign_table datacampaign(get_self(), get_self().value);
          auto existing_memo = datacampaign.get_index<"bymemo"_n>();
          auto itr = existing_memo.find(memo_value);

          if (itr != existing_memo.end()) {
              time_point_sec current_time = current_time_point();
              uint64_t current_time_sec = current_time.sec_since_epoch();

              if (current_time_sec < itr->start) {
                  eosio::check(false, "Campaign has not yet started");
              }
              
              if (current_time_sec > itr->finish) {
                  eosio::check(false, "This campaign has already ended");
              }

              if (quantity != itr->entry) {
                  eosio::check(false, "Number of tokens does not match specified entry");
              }

              if (itr->printondemand == 0) {
                stakers_table stakers_table(get_self(), get_self().value);
                stakers_table.emplace(get_self(), [&](auto& row) {
                  row.participant = from;
                  row.campaign = itr->campaign;
                  row.join_time = current_time.sec_since_epoch();
                  row.claimable_reward = current_time.sec_since_epoch() + itr->timetoreward;
                  row.claimed = false;
                  row.retired = false;

                  rewards_table rewards(get_self(), get_self().value);
                  auto rewards_itr = rewards.get_index<"bycampaign"_n>();
                  auto rewards_entry = rewards_itr.find(itr->campaign.value);
                  while (rewards_entry != rewards_itr.end() && rewards_entry->available == false) {
                      rewards_entry++;
                  }
                  if (rewards_entry != rewards_itr.end()) {
                      row.id_asigned = rewards_entry->id;
                      rewards_itr.modify(rewards_entry, get_self(), [&](auto& r) {
                          r.available = false;
                      });
                  } else {
                      eosio::check(false, "No reward was found available for this campaign.");
                  }
                });

              }
              else if (itr->printondemand == 1) {
                eosio::check(false, "Printondemand does not require filling");
              }
          } else {
              eosio::check(false, "Unable to join campaign");
          }
      }
  }



  // Allows the reward to be claimed after the stake time has expired
    //user - user claiming his reward
  [[eosio::action]]
  void claimreward(name user) {
      require_auth(user);

      stakers_table stakers(get_self(), get_self().value);
      auto staker_entry = stakers.find(user.value);

      if (staker_entry != stakers.end()) {
          if (staker_entry->claimed || staker_entry->retired) {
          eosio::check(false, "Already claimed or withdrawn from this campaign");
          }

          
          time_point_sec current_time = current_time_point();
          uint64_t current_time_sec = current_time.sec_since_epoch();
          
          uint64_t claimable_reward_time = staker_entry->claimable_reward;

          if (current_time_sec < claimable_reward_time) {
              eosio::check(false, "You have not completed stake time yet!");
          }

          name campaign = staker_entry->campaign;

          datacampaign_table datacampaign(get_self(), get_self().value);
          auto datacampaign_entry = datacampaign.find(campaign.value);

          if (datacampaign_entry != datacampaign.end()) {
              bool islimited = datacampaign_entry->islimited;
              bool printondemand = datacampaign_entry->printondemand;
              bool return_entry = datacampaign_entry->return_entry;

              asset entry_value = datacampaign_entry->entry;

              name token_account = datacampaign_entry->tokenaccount;

              if (islimited) {
                  name external_contract = datacampaign_entry->nftaccount;
                  uint64_t nftreward = staker_entry->id_asigned;

                  std::vector<uint64_t> assetids_str;
                  assetids_str.push_back(nftreward);

                if (return_entry == 0) {
                  action(
                    permission_level{get_self(), "active"_n},
                    external_contract, 
                    "transfer"_n,     
                    std::make_tuple(get_self(), user, assetids_str, std::string("NFT claimed"))
                  ).send();


                } else {
                
                   action(
                    permission_level{get_self(), "active"_n},
                    external_contract, 
                    "transfer"_n,     
                    std::make_tuple(get_self(), user, assetids_str, std::string("NFT claimed"))
                  ).send();

                  action(
                    permission_level{get_self(), "active"_n},
                    token_account, 
                    "transfer"_n,
                    std::make_tuple(get_self(), user, entry_value, std::string("Returned entry"))
                   ).send();
                  }

                  stakers.modify(staker_entry, get_self(), [&](auto& row) {
                      row.claimed = true;
                  });

                  rewards_table rewards(get_self(), get_self().value);
                  auto rewards_entry = rewards.find(staker_entry->id_asigned);

                  if (rewards_entry != rewards.end()) {
                      rewards.modify(rewards_entry, get_self(), [&](auto& row) {
                          row.delivered = true;
                      });
                  } else {
                      eosio::check(false, "No corresponding entry was found in rewards table.");
                  }


              } else if (printondemand) {
                  // TODO 

              } else {
                  eosio::check(false, "The campaign has no valid configuration");
              }
            } else {
              eosio::check(false, "No corresponding campaign found in datacampaign");
            }
      } else {
          eosio::check(false, "You are not participating in any campaign");
      }
  }


 
  // Allows withdrawing from campaign only if stake time has not been completed
    //user - user who withdraws his participation
  [[eosio::action]]
  void retirestake(name user) {
      require_auth(user);

      stakers_table stakers(get_self(), get_self().value);
      auto staker_entry = stakers.find(user.value);

      if (staker_entry != stakers.end()) {
          if (staker_entry->claimed || staker_entry->retired) {
              eosio::check(false, "Already claimed or withdrawn from this campaign");
          }

          time_point_sec current_time = current_time_point();
          uint64_t current_time_sec = current_time.sec_since_epoch();

          if (current_time_sec < staker_entry->claimable_reward) {
              name campaign = staker_entry->campaign;

              datacampaign_table datacampaign(get_self(), get_self().value);
              auto datacampaign_entry = datacampaign.find(campaign.value);

              if (datacampaign_entry != datacampaign.end()) {
                  asset entry_value = datacampaign_entry->entry;
                  name token_account = datacampaign_entry->tokenaccount;

                  action(
                    permission_level{get_self(), "active"_n},
                    token_account, 
                    "transfer"_n,  
                    std::make_tuple(get_self(), user, entry_value, std::string("Returned entry"))
                  ).send();

                  stakers.modify(staker_entry, user, [&](auto &staker) {
                  staker.retired = true;
                  });

                  rewards_table rewards(get_self(), get_self().value);
                  auto reward_entry = rewards.find(staker_entry->id_asigned);
                  if (reward_entry != rewards.end()) {
                  rewards.modify(reward_entry, user, [&](auto &reward) {
                  reward.available = true;
              });
                  }

              } else {
                  eosio::check(false, "No matching entry found in datacampaign table");
              }

          } else {
              eosio::check(false, "Stake completed, you must claim your reward");
          }
      } else {
          eosio::check(false, "No matching entry was found in stakers table.");
      }
  }


  
  // Allows you to delete all "Stakers" of the same campaign.
    //camptoclear - The campaign in which you want to delete all stakers
    //memo - "Confirm" for security
  [[eosio::action]]
  void delstakers(name camptoclear, std::string memo) {
      require_auth(get_self());

      eosio::check(memo == "confirm", "Fail.");

      stakers_table stakers(get_self(), get_self().value);

      auto staker_entry = stakers.begin();
      auto staker_end = stakers.end();

      bool foundEntries = false;

      while (staker_entry != staker_end) {
          if (staker_entry->campaign == camptoclear) {
              staker_entry = stakers.erase(staker_entry);
              foundEntries = true; 
          } else {
              staker_entry++;
          }
      }
      eosio::check(foundEntries, "No entries found for specified campaign.");
  }





private:
  struct [[eosio::table]] datacampaign {
    name campaign;
    name filler;
    uint64_t start;
    uint64_t finish;
    uint64_t timetoreward;
    name nftaccount;
    name tokenaccount;
    asset entry;
    bool return_entry;
    uint64_t places;
    bool islimited;
    bool printondemand;
    uint64_t memo_expected;

    uint64_t primary_key() const { return campaign.value; }
    uint64_t by_memo_expected() const { return memo_expected; }
  };


  struct [[eosio::table]] rewards {
    uint64_t id;          
    name campaign;        
    bool available;       
    bool delivered;       

    uint64_t primary_key() const { return id; }
    uint64_t by_campaign() const { return campaign.value; }
  };


  struct [[eosio::table]] stakers {
    name participant;
    name campaign;
    uint64_t join_time;
    uint64_t claimable_reward;
    bool claimed;
    bool retired; 
    uint64_t id_asigned;

    uint64_t primary_key() const { return participant.value; }
    uint64_t by_campaign() const { return campaign.value; }
  };


  typedef eosio::multi_index<"datacampaign"_n, datacampaign, 
    indexed_by<"bymemo"_n, const_mem_fun<datacampaign, uint64_t, &datacampaign::by_memo_expected>>
  > datacampaign_table;


  typedef eosio::multi_index<"rewards"_n, rewards,
    indexed_by<"bycampaign"_n, const_mem_fun<rewards, uint64_t, &rewards::by_campaign>>
  > rewards_table;


  typedef eosio::multi_index<"stakers"_n, stakers,
    indexed_by<"bycampaign"_n, const_mem_fun<stakers, uint64_t, &stakers::by_campaign>>
  > stakers_table;

};


