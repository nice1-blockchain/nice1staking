#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/symbol.hpp>
#include <eosio/string.hpp>
#include <eosio/system.hpp>

using namespace eosio;

class [[eosio::contract("n1nfttotoken")]] n1nfttotoken : public contract {
public:
  using contract::contract;


// Set up a new stake campaign
    //campaign - Name of the campaign in the table
    //start - UNIX time to start the campaign
    //finish - UNIX time to end the campaign
    //timetoreward - UNIX time needed to claim rewards
    //nftaccount - Contract that manages the NFT used in the campaign
    //tokenaccount - Contract that handles Tokens used in the campaign
    //reward - Number of reward tokens
    //places - Maximum number of participants in the campaign
    //memo_expected - Used as a match for the contract logics  
  
  [[eosio::action]]
  void setcampaign(name campaign, uint64_t start, uint64_t finish,
                 uint64_t timetoreward, name nftaccount, name tokenaccount,
                 asset reward, uint64_t places, uint64_t memo_expected)  {
    require_auth(get_self());

    
    campaigndata_table campaigndata(get_self(), get_self().value);
    auto existing = campaigndata.find(campaign.value);
    check(existing == campaigndata.end(), "The campaign already exists in the table campaigndata");

    
    check(memo_expected > 0, "memo_expected must be a valid number");

    
    check(places > 0, "Value of 'places' cannot be equal to 0");
    
    
    auto existing_memo = campaigndata.get_index<"bymemo"_n>();
    auto itr = existing_memo.find(memo_expected);
    check(itr == existing_memo.end(), "Already an entry with the same memo_expected");

    
    time_point_sec current_time = current_time_point();
    uint64_t current_time_sec = current_time.sec_since_epoch();

    
    check(start >= current_time.sec_since_epoch(), "Date specified in 'start' has already passed and is invalid.");
    
    
    check(start < finish, "Start time must be before the end time.");
    
    
    check(timetoreward < (finish - start), "Duration is greater to active time");

    
    campaigndata.emplace(get_self(), [&](auto& row) {
      row.campaign = campaign;
      row.start = start;
      row.finish = finish;
      row.timetoreward = timetoreward;
      row.nftaccount = nftaccount;
      row.tokenaccount = tokenaccount;
      row.places = places;
      row.reward = reward;
      row.memo_expected = memo_expected;
    });
  }

// Configure nft data to be received
    //campaign - Campaign name to which to add the info
    //author - Expected nft author information
    //category - Expected nft category information
    //idata - Expected nft idata information

  [[eosio::action]]
void addnftdata(name campaign, name author, name category, string idata) {
    require_auth(get_self());

    
    campaigndata_table campaigndata(get_self(), get_self().value);
    auto campaign_itr = campaigndata.find(campaign.value);
    check(campaign_itr != campaigndata.end(), "There is no campaign with this name");

    
    nftdata_table nftdata(get_self(), get_self().value);
    auto nftdata_itr = nftdata.find(campaign.value);
    check(nftdata_itr == nftdata.end(), "An entry with this campaign already exists in the table nftdata");

    nftdata.emplace(get_self(), [&](auto& row) {
        row.campaign = campaign;
        row.author = author;
        row.category = category;
        row.idata = idata;
    });
}


  // Allows the reward to be claimed after the stake time has expired
    //user - user claiming his reward

[[eosio::action]]
void claimreward(name user) {
    require_auth(user);

    
    stakers_table stakers(get_self(), get_self().value);
    auto staker_itr = stakers.find(user.value);
    check(staker_itr != stakers.end(), "You are not registered as a participant in any campaign");

    check(staker_itr->claimed == false && staker_itr->retired == false, "Already claimed or withdrawn from this campaign");

    
    time_point_sec current_time = current_time_point();
    uint64_t current_time_sec = current_time.sec_since_epoch();
    uint64_t user_claimable_reward = staker_itr->claimable_reward;

    check(current_time_sec >= user_claimable_reward, "You have not completed stake time yet!");

    
   
    uint64_t user_id_nft = staker_itr->id_nft;

    std::vector<uint64_t> user_id_nft_vector;
    user_id_nft_vector.push_back(user_id_nft);


    
    name user_campaign = staker_itr->campaign;
    
    campaigndata_table campaigndata(get_self(), get_self().value);
    auto campaign_itr = campaigndata.find(user_campaign.value);
    check(campaign_itr != campaigndata.end(), "Corresponding campaign was not found in the campaigndata table.");

    name nft_account = campaign_itr->nftaccount;
    name token_account = campaign_itr->tokenaccount;
    asset rewardclaim = campaign_itr->reward;

    action(
        permission_level{get_self(), "active"_n},
        nft_account,
        "transfer"_n,
        std::make_tuple(get_self(), user, user_id_nft_vector, std::string("NFT returned"))
    ).send();

    action(
        permission_level{get_self(), "active"_n},
        token_account,
        "transfer"_n,
        std::make_tuple(get_self(), user, rewardclaim, std::string("Tokens claimed"))
    ).send();

     
    stakers.modify(staker_itr, get_self(), [&](auto& row) {
    row.claimed = true;

});  
}


  // Allows withdrawing from campaign only if stake time has not been completed
    //user - user who withdraws his participation

[[eosio::action]]
void retirestake(name user) {
    require_auth(user);

    
    stakers_table stakers(get_self(), get_self().value);
    auto staker_itr = stakers.find(user.value);
    check(staker_itr != stakers.end(), "You are not registered as a participant in any campaign");

    check(staker_itr->claimed == false && staker_itr->retired == false, "Already claimed or withdrawn from this campaign");

    
    time_point_sec current_time = current_time_point();
    uint64_t current_time_sec = current_time.sec_since_epoch();
    uint64_t user_claimable_reward = staker_itr->claimable_reward;

    check(current_time_sec < user_claimable_reward, "You cannot withdraw, claim your reward.");

    
    uint64_t user_id_nft = staker_itr->id_nft;

    std::vector<uint64_t> user_id_nft_vector;
    user_id_nft_vector.push_back(user_id_nft);

    
    name user_campaign = staker_itr->campaign;

    campaigndata_table campaigndata(get_self(), get_self().value);
    auto campaign_itr = campaigndata.find(user_campaign.value);
    check(campaign_itr != campaigndata.end(), "Corresponding campaign was not found in the campaigndata table.");

    name nft_account = campaign_itr->nftaccount;

    action(
        permission_level{get_self(), "active"_n},
        nft_account,
        "transfer"_n,
        std::make_tuple(get_self(), user, user_id_nft_vector, std::string("NFT returned"))
    ).send();


    
    stakers.modify(staker_itr, get_self(), [&](auto& row) {
        row.retired = true;
    });
}



 // Allows you to delete an entry from datacampaign.
    //campaign - Campaign you want to delete from table.
    //memo - "Confirm" for security

[[eosio::action]]
void delcampaign(name campaign, string memo) {
    require_auth(get_self());

    
    check(memo == "confirm", "Fail");

    
    campaigndata_table campaigndata(get_self(), get_self().value);
    auto campaign_itr = campaigndata.find(campaign.value);
    check(campaign_itr != campaigndata.end(), "The campaign does not exist in the table campaigndata");

    
    campaigndata.erase(campaign_itr);
}



 // Allows you to delete an entry from nftdata.
    //campaign - Campaign you want to delete from table.
    //memo - "Confirm" for security

[[eosio::action]]
void delnftdata(name campaign, string memo) {
    require_auth(get_self());

    
    check(memo == "confirm", "Fail");

    
    nftdata_table nftdata(get_self(), get_self().value);
    auto nftdata_itr = nftdata.find(campaign.value);
    check(nftdata_itr != nftdata.end(), "The campaign does not exist in the table nftdata");

   
    nftdata.erase(nftdata_itr);
}




 // Allows you to delete all "Stakers" of the same campaign.
    //camptoclear - Campaign in which you want to delete all stakers
    //memo - "Confirm" for security

[[eosio::action]]
void delstakers(name campaign, string memo) {
    require_auth(get_self());

    
    check(memo == "confirm", "Fail");

   
    stakers_table stakers(get_self(), get_self().value);
    auto staker_itr = stakers.begin();
    while (staker_itr != stakers.end()) {
        if (staker_itr->campaign == campaign) {
            staker_itr = stakers.erase(staker_itr);
        } else {
            ++staker_itr;
        }
    }
}


//Logic to receive the nft and to be registered in the campaign as a staker.

[[eosio::on_notify("simpleassets::transfer")]]
void nft_transfer(name from, name to, std::vector<uint64_t>& assetids, std::string memo) {
    if (to == get_self()) {
        
        check(!memo.empty(), "Memo must not be empty");

       
        campaigndata_table campaigndata(get_self(), get_self().value);
        auto by_memo_index = campaigndata.get_index<"bymemo"_n>();
        auto campaign_itr = by_memo_index.find(stoull(memo));

       
        if (campaign_itr != by_memo_index.end()) {
           

            
            name matching_campaign_name = campaign_itr->campaign;

            
            nftdata_table nftdata(get_self(), get_self().value);
            auto nftdata_itr = nftdata.find(matching_campaign_name.value);

            
            if (nftdata_itr != nftdata.end()) {
             }

            
            time_point_sec current_time = current_time_point();
            uint64_t current_time_sec = current_time.sec_since_epoch();
            uint64_t campaign_start = campaign_itr->start;
            uint64_t campaign_finish = campaign_itr->finish;

            eosio::check(current_time_sec > campaign_start, "Campaign has not yet started.");
            eosio::check(current_time_sec < campaign_finish, "This campaign has already ended.");

            name nft_account = campaign_itr->nftaccount;
            name sender_contract = get_first_receiver();
            eosio::check(sender_contract == nft_account, "Invalid nft contract");

            std::vector<uint64_t> asset_ids = assetids;
            eosio::check(asset_ids.size() == 1, "Only one ID was expected");
            uint64_t transaction_id = asset_ids[0];

            
            sassets_table sassets("simpleassets"_n, get_self().value);
            auto asset_itr = sassets.find(transaction_id);

            
            if (asset_itr != sassets.end()) {
                
                eosio::check(nftdata_itr->author == asset_itr->author, "The 'author' data do not match");
                eosio::check(nftdata_itr->category == asset_itr->category, "The 'category' data do not match");
                eosio::check(nftdata_itr->idata == asset_itr->idata, "The 'idata' data do not match");

                
               stakers_table stakers(get_self(), get_self().value);
               auto staker_itr = stakers.find(from.value);

               
               if (staker_itr != stakers.end()) {
                  eosio::check(false, "You are already participating or have participated.");
               }


              
               bool nft_already_participated = false;
               for (auto itr = stakers.begin(); itr != stakers.end(); ++itr) {
                  if (itr->id_nft == transaction_id) {
                     nft_already_participated = true;
                     break;  
                  }
               }

               eosio::check(!nft_already_participated, "This NFT has already participated");


               
               uint64_t places = campaign_itr->places; 

              
               uint64_t count = 0;
               for (auto itr = stakers.begin(); itr != stakers.end(); ++itr) {
                  if (itr->campaign == matching_campaign_name) {
                     count++;
                  }
                }

              
               if (count >= places) {
                  eosio::check(false, "There are no places for this campaign.");
               }


               
               uint64_t timetoreward = campaign_itr->timetoreward;
               uint64_t claimable_reward = current_time_sec + timetoreward;

               
               if (staker_itr == stakers.end()) {
               stakers.emplace(get_self(), [&](auto& row) {
                  row.participant = from;
                  row.campaign = matching_campaign_name;
                  row.join_time = current_time_sec;
                  row.claimable_reward = claimable_reward;
                  row.claimed = false; 
                  row.retired = false; 
                  row.id_nft = transaction_id; 
               });
            }
                
               
            } else {
                
                eosio::check(false, "The asset_id was not found in the table 'sassets'.");
            }
        } else {
            
            eosio::check(false, "The campaign does not exist");
        }
    }
}



private:

  
  struct [[eosio::table]] staking_config {
    name campaign;
    uint64_t start;
    uint64_t finish;
    uint64_t timetoreward;
    name nftaccount;
    name tokenaccount;
    asset reward;
    uint64_t places;
    uint64_t memo_expected;

    uint64_t primary_key() const { return campaign.value; }
    uint64_t by_memo() const { return memo_expected; }
  };



  
  struct [[eosio::table]] nft_data {
    name campaign;
    name author;
    name category;
    string idata;

    uint64_t primary_key() const { return campaign.value; }
  };


struct [[eosio::table]] staker {
    name participant;
    name campaign;
    uint64_t join_time;
    uint64_t claimable_reward;
    bool claimed;
    bool retired;
    uint64_t id_nft;

    uint64_t primary_key() const { return participant.value; }
    uint64_t by_campaign() const { return campaign.value; }
};




 
  typedef eosio::multi_index<"campaigndata"_n, staking_config,
    indexed_by<"bymemo"_n, const_mem_fun<staking_config, uint64_t, &staking_config::by_memo>>
  > campaigndata_table;


  typedef eosio::multi_index<"nftdata"_n, nft_data> nftdata_table;

  typedef eosio::multi_index<"stakers"_n, staker,
    indexed_by<"bycampaign"_n, const_mem_fun<staker, uint64_t, &staker::by_campaign>>
  > stakers_table;

 


   struct [[eosio::table]] sassets {
    uint64_t id;
    name owner;
    name author;
    name category;
    string idata;
   
    uint64_t primary_key() const { return id; }
  };

  typedef multi_index<"sassets"_n, sassets> sassets_table;
  
};