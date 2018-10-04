
/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#include <utility>
#include <vector>
#include <string>
#include <eosiolib/eosio.hpp>
#include <eosiolib/time.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/contract.hpp>
#include <eosiolib/crypto.h>

using std::string;
using eosio::key256;
using eosio::indexed_by;
using eosio::const_mem_fun;
using eosio::asset;
using eosio::permission_level;
using eosio::action;
using eosio::print;
using eosio::name;

class raffle : public eosio::contract {
 
   public:
     
      const uint32_t DAY = 24*60*60;

      raffle(account_name self):eosio::contract(self), raffle(_self, _self), games(_self, _self), global_dices(_self, _self),
       accounts(_self, _self)
      {}

      //@abi action
      void createraffle(const account_name owner, 
                        const asset& prize, 
                        uint64_t n_of_tikects,  
                        const string& prize,
                        const string& raffle_desc){


        eosio_assert( prize.symbol == CORE_SYMBOL, "only core token allowed" );
        eosio_assert( prize.is_valid(), "invalid prize" );
        eosio_assert( prize.amount > 0, "must prize positive quantity" );
        eosio_assert( n_of_tikects > 0, "number of tickets must to be a positive quantity" );

        require_auth( _self );

        // Store new raffle
         auto new_raffle_itr = raffles.emplace(_self, [&](auto& offer){
            raffle.id           = offers.available_primary_key();
            raffle.owner        = bet;
            raffle.prize        = player;
            raffle.n_of_tikects = commitment;
            raffle.ticket_price =   // calculated 
            raffle.raffle_desc  = 0;
            raffle.deadline = eosio::time_point_sec(now() + DAY)
         });

      // Create many colored names to raffle list


      // Broadcast to blockchain a sorted colored name criptografed by actual active key and save txid

      }


      //@abi action
      void buyticket( const account_name from, const asset& quantity ) {
         
         eosio_assert( quantity.is_valid(), "invalid quantity" );
         eosio_assert( quantity.amount > 0, "must deposit positive quantity" );

         auto itr = accounts.find(from);
         if( itr == accounts.end() ) {
            itr = accounts.emplace(_self, [&](auto& acnt){
               acnt.owner = from;
            });
         }

         action(
            permission_level{ from, N(active) },
            N(eosio.token), N(transfer),
            std::make_tuple(from, _self, quantity, std::string("RafflEos.io - Raffle Ticket"))
         ).send();

         accounts.modify( itr, 0, [&]( auto& acnt ) {
            acnt.eos_balance += quantity;
         });
      }
   

      //@abi action
      void canceloffer( const checksum256& commitment ) {

         auto idx = offers.template get_index<N(commitment)>();
         auto offer_itr = idx.find( offer::get_commitment(commitment) );

         eosio_assert( offer_itr != idx.end(), "offer does not exists" );
         eosio_assert( offer_itr->gameid == 0, "unable to cancel offer" );
         require_auth( offer_itr->owner );

         auto acnt_itr = accounts.find(offer_itr->owner);
         accounts.modify(acnt_itr, 0, [&](auto& acnt){
            acnt.open_offers--;
            acnt.eos_balance += offer_itr->bet;
         });

         idx.erase(offer_itr);
      }

      //@abi action
      void reveal( const checksum256& commitment, const checksum256& source ) {

         assert_sha256( (char *)&source, sizeof(source), (const checksum256 *)&commitment );

         auto idx = offers.template get_index<N(commitment)>();
         auto curr_revealer_offer = idx.find( offer::get_commitment(commitment)  );

         eosio_assert(curr_revealer_offer != idx.end(), "offer not found");
         eosio_assert(curr_revealer_offer->gameid > 0, "unable to reveal");

         auto game_itr = games.find( curr_revealer_offer->gameid );

         player curr_reveal = game_itr->player1;
         player prev_reveal = game_itr->player2;

         if( !is_equal(curr_reveal.commitment, commitment) ) {
            std::swap(curr_reveal, prev_reveal);
         }

         eosio_assert( is_zero(curr_reveal.reveal) == true, "player already revealed");

         if( !is_zero(prev_reveal.reveal) ) {

            checksum256 result;
            sha256( (char *)&game_itr->player1, sizeof(player)*2, &result);

            auto prev_revealer_offer = idx.find( offer::get_commitment(prev_reveal.commitment) );

            int winner = result.hash[1] < result.hash[0] ? 0 : 1;

            if( winner ) {
               pay_and_clean(*game_itr, *curr_revealer_offer, *prev_revealer_offer);
            } else {
               pay_and_clean(*game_itr, *prev_revealer_offer, *curr_revealer_offer);
            }

         } else {
            games.modify(game_itr, 0, [&](auto& game){

               if( is_equal(curr_reveal.commitment, game.player1.commitment) )
                  game.player1.reveal = source;
               else
                  game.player2.reveal = source;

               game.deadline = eosio::time_point_sec(now() + FIVE_MINUTES);
            });
         }
      }

      //@abi action
      void claimexpired( const uint64_t gameid ) {

         auto game_itr = games.find(gameid);

         eosio_assert(game_itr != games.end(), "game not found");
         eosio_assert(game_itr->deadline != eosio::time_point_sec(0) && eosio::time_point_sec(now()) > game_itr->deadline, "game not expired");

         auto idx = offers.template get_index<N(commitment)>();
         auto player1_offer = idx.find( offer::get_commitment(game_itr->player1.commitment) );
         auto player2_offer = idx.find( offer::get_commitment(game_itr->player2.commitment) );

         if( !is_zero(game_itr->player1.reveal) ) {
            eosio_assert( is_zero(game_itr->player2.reveal), "game error");
            pay_and_clean(*game_itr, *player1_offer, *player2_offer);
         } else {
            eosio_assert( is_zero(game_itr->player1.reveal), "game error");
            pay_and_clean(*game_itr, *player2_offer, *player1_offer);
         }

      }



      //@abi action
      void withdraw( const account_name to, const asset& quantity ) {
         require_auth( to );

         eosio_assert( quantity.is_valid(), "invalid quantity" );
         eosio_assert( quantity.amount > 0, "must withdraw positive quantity" );

         auto itr = accounts.find( to );
         eosio_assert(itr != accounts.end(), "unknown account");

         accounts.modify( itr, 0, [&]( auto& acnt ) {
            eosio_assert( acnt.eos_balance >= quantity, "insufficient balance" );
            acnt.eos_balance -= quantity;
         });

         action(
            permission_level{ _self, N(active) },
            N(eosio.token), N(transfer),
            std::make_tuple(_self, to, quantity, std::string(""))
         ).send();

         if( itr->is_empty() ) {
            accounts.erase(itr);
         }
      }

   private:

          void colorednames( const account_name voucher_issuer, 
                            const account_name voucher_owner,
                            uint64_t offerId)
        {
            auto id = _voucher.available_primary_key();
            
             _voucher.emplace(get_self(), [&](auto &voucher) {
                voucher.voucherId = id;
                voucher.offerId = offerId;
                voucher.voucher_issuer = voucher_issuer;
                voucher.voucher_owner = voucher_owner;
                voucher.buy_date = now();
            });
            print("Your voucherId is: ");
            print(id);

        }
        

      //@abi table raffles i64
      struct raffles {
         uint64_t          id;
         account_name      owner;
         asset             prize;
         uint64_t          n_of_tikects
         asset             ticket_price
         string            raffle_desc;
         eosio::time_point_sec deadline;


         uint64_t primary_key()const { return id; }

         uint64_t by_prize()const { return (uint64_t)prize.amount; }

         EOSLIB_SERIALIZE( raffles, (id)(owner)(prize)(n_of_tikects)(ticket_price)(raffle_desc)(raffleid) )
      };

      typedef eosio::multi_index< N(raffles), raffles,
         indexed_by< N(prize), const_mem_fun<raffles, uint64_t, &raffles::by_prize > >,
         indexed_by< N(ticket_price), const_mem_fun<raffles, key256,  &raffles::by_ticket_price> >
      > raffles_index;

      //@abi table names i64
      struct names {
         uint64_t       id;
         uint64_t       raffle_id;
         string         colored_name;
         account_name   owner;

         uint64_t primary_key()const { return id; }

         EOSLIB_SERIALIZE( game, (id)(bet)(deadline)(player1)(player2) )
      };

     
     
     
     
     
     
     
     
     
     
      struct player {
         checksum256 commitment;
         checksum256 reveal;

         EOSLIB_SERIALIZE( player, (commitment)(reveal) )
      };

      //@abi table game i64
      struct game {
         uint64_t id;
         asset    bet;
         eosio::time_point_sec deadline;
         player   player1;
         player   player2;

         uint64_t primary_key()const { return id; }

         EOSLIB_SERIALIZE( game, (id)(bet)(deadline)(player1)(player2) )
      };

      typedef eosio::multi_index< N(game), game> game_index;

      //@abi table global i64
      struct global_dice {
         uint64_t id = 0;
         uint64_t nextgameid = 0;

         uint64_t primary_key()const { return id; }

         EOSLIB_SERIALIZE( global_dice, (id)(nextgameid) )
      };

      typedef eosio::multi_index< N(global), global_dice> global_dice_index;

      //@abi table account i64
      struct account {
         account( account_name o = account_name() ):owner(o){}

         account_name owner;
         asset        eos_balance;
         uint32_t     open_offers = 0;
         uint32_t     open_games = 0;

         bool is_empty()const { return !( eos_balance.amount | open_offers | open_games ); }

         uint64_t primary_key()const { return owner; }

         EOSLIB_SERIALIZE( account, (owner)(eos_balance)(open_offers)(open_games) )
      };

      typedef eosio::multi_index< N(account), account> account_index;

      raffle_index      raffle;
      game_index        games;
      global_dice_index global_dices;
      account_index     accounts;

      bool has_offer( const checksum256& commitment )const {
         auto idx = offers.template get_index<N(commitment)>();
         auto itr = idx.find( offer::get_commitment(commitment) );
         return itr != idx.end();
      }

      bool is_equal(const checksum256& a, const checksum256& b)const {
         return memcmp((void *)&a, (const void *)&b, sizeof(checksum256)) == 0;
      }

      bool is_zero(const checksum256& a)const {
         const uint64_t *p64 = reinterpret_cast<const uint64_t*>(&a);
         return p64[0] == 0 && p64[1] == 0 && p64[2] == 0 && p64[3] == 0;
      }

      void pay_and_clean(const game& g, const offer& winner_offer,
          const offer& loser_offer) {

         // Update winner account balance and game count
         auto winner_account = accounts.find(winner_offer.owner);
         accounts.modify( winner_account, 0, [&]( auto& acnt ) {
            acnt.eos_balance += 2*g.bet;
            acnt.open_games--;
         });

         // Update losser account game count
         auto loser_account = accounts.find(loser_offer.owner);
         accounts.modify( loser_account, 0, [&]( auto& acnt ) {
            acnt.open_games--;
         });

         if( loser_account->is_empty() ) {
            accounts.erase(loser_account);
         }

         games.erase(g);
         offers.erase(winner_offer);
         offers.erase(loser_offer);
      }
};

EOSIO_ABI( raffle, (offerbet)(canceloffer)(reveal)(claimexpired)(deposit)(withdraw) )
