
/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */

#include <iostream>  // std::cout
#include <algorithm> // std::random_shuffle
#include <vector>    // std::vector
#include <ctime>     // std::time
#include <cstdlib>   // std::rand, std::srand
#include <string>    // std:string
#include <utility>
#include <eosiolib/eosio.hpp>
#include <eosiolib/time.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/contract.hpp>
#include <eosiolib/crypto.h>

using eosio::action;
using eosio::asset;
using eosio::const_mem_fun;
using eosio::indexed_by;
using eosio::key256;
using eosio::name;
using eosio::permission_level;
using eosio::print;
using std::string;

class raffle : public eosio::contract
{

  public:
    const uint32_t MINUTE = 60;

    // random generator function:
    int raffle_random(int i) { return std::rand() % i; }

    raffle(account_name self) : eosio::contract(self), raffle(_self, _self), games(_self, _self), global_dices(_self, _self),
                                accounts(_self, _self)
    {
    }

    //@abi action
    void createraffle(const account_name owner,
                      const asset& prize,
                      uint64_t n_of_tikects,
                      const string& prize,
                      const string& raffle_desc)
    {

        eosio_assert(prize.symbol == CORE_SYMBOL, "only core token allowed");
        eosio_assert(prize.is_valid(), "invalid prize");
        eosio_assert(prize.amount > 0, "must prize positive quantity");
        eosio_assert(n_of_tikects > 0, "number of tickets must to be a positive quantity");

        require_auth(_self);

        rafflestable raffles(_self, _self);
        auto itr1 = raffles.find(owner);

        auto raffleid = raffle.available_primary_key();

        // Store the new raffle
        auto new_raffle_itr = raffles.emplace(_self, [&](auto& raffle) {
            raffle.id = raffleid;
            raffle.owner = owner;
            raffle.prize = prize;
            raffle.n_of_tikects = commitment;
            raffle.ticket_price = 0; // calculated
            raffle.raffle_desc = raffle_desc;
            raffle.deadline = eosio::time_point_sec(now() + MINUTE) // Time to close the raffle an reveal the secret
        });

        // Create many colored number to raffle list
        std::srand(unsigned(std::time(0)));
        std::vector<string> raffle_list;

        // Initialize 2D array
        char colour[4][10] = {"Blue", "Red", "Green", "Yellow"};

        // Generate Colored Number List
        for (int i = 0; i < 4; i++)
        {

            for (int j = 500; j <= 501; j++)
            {
                auto s = std::to_string(j);
                raffle_list.push_back(colour[i] + s);

                // Store a colored number
                numberstable numbers(_self, _self);
                auto itr2 = numbers.begin();

                auto new_number_itr = numbers.emplace(_self, [&](auto& number) {
                    number.id = number.available_primary_key();;
                    number.raffle_id = raffleid;
                    number.colored_name = player;
                });
            }
        }

    }

        void buyticket(const account_name from , uint64_t numberId)
    {

        eosio_assert(quantity.is_valid(), "invalid quantity");
        eosio_assert(quantity.amount > 0, "must deposit positive quantity");

        auto itr = numbers.find(numberId);
        if (itr == numbers.end())
        {
             numbers.modify(itr, 0, [&](auto& number) {
                number.owner = from;
             });
        }
/*
        action(
            permission_level{from, N(active)},
            N(eosio.token), N(transfer),
            std::make_tuple(from, _self, quantity, std::string("RafflEos.io - Raffle Ticket")))
            .send();
*/
       
    }


    //@abi action
    void raffle(uint64_t raffleid)
    {

        auto raffle_itr = raffles.find(raffleid);

        eosio_assert(raffle_itr != games.end(), "raffle not found");
        eosio_assert(raffle_itr->deadline != eosio::time_point_sec(0) && eosio::time_point_sec(now()) > raffle_itr->deadline, "raffle not expired");

        require_auth(_self);

        // Sorting the colored number list
        auto raffle_list = std::random_shuffle(raffles.begin(), raffles.end(), raffle_random);

        // resizes the vector size to 4 
        auto sorted_c_name = raffle_list.resize(1);

        // Broadcast to blockchain a sorted colored number criptografed by actual active key and save txid
        print(sorted_c_name);

        // Find the winner
        auto winner_itr = numbers.find(sorted_c_name);
        auto winner = winner_itr->owner;

        print (winner);
    }

    /*

    void claimexpired(const uint64_t gameid)
    {

        auto game_itr = games.find(gameid);

        eosio_assert(game_itr != games.end(), "game not found");
        eosio_assert(game_itr->deadline != eosio::time_point_sec(0) && eosio::time_point_sec(now()) > game_itr->deadline, "game not expired");

        auto idx = offers.template get_index<N(commitment)>();
        auto player1_offer = idx.find(offer::get_commitment(game_itr->player1.commitment));
        auto player2_offer = idx.find(offer::get_commitment(game_itr->player2.commitment));

        if (!is_zero(game_itr->player1.reveal))
        {
            eosio_assert(is_zero(game_itr->player2.reveal), "game error");
            pay_and_clean(*game_itr, *player1_offer, *player2_offer);
        }
        else
        {
            eosio_assert(is_zero(game_itr->player1.reveal), "game error");
            pay_and_clean(*game_itr, *player2_offer, *player1_offer);
        }
    }

    void withdraw(const account_name to, const asset &quantity)
    {
        require_auth(to);

        eosio_assert(quantity.is_valid(), "invalid quantity");
        eosio_assert(quantity.amount > 0, "must withdraw positive quantity");

        auto itr = accounts.find(to);
        eosio_assert(itr != accounts.end(), "unknown account");

        accounts.modify(itr, 0, [&](auto &acnt) {
            eosio_assert(acnt.eos_balance >= quantity, "insufficient balance");
            acnt.eos_balance -= quantity;
        });

        action(
            permission_level{_self, N(active)},
            N(eosio.token), N(transfer),
            std::make_tuple(_self, to, quantity, std::string("")))
            .send();

        if (itr->is_empty())
        {
            accounts.erase(itr);
        }
    }
    */
  private:

    //@abi table raffles i64
    struct raffles
    {
        uint64_t id;
        account_name owner;
        asset prize;
        uint64_t n_of_tikects
        asset ticket_price;
        string raffle_desc;
        eosio::time_point_sec deadline;

        uint64_t primary_key() const { return id; }
        uint64_t by_prize() const { return (uint64_t)prize.amount; }

        EOSLIB_SERIALIZE(raffles, (id)(owner)(prize)(n_of_tikects)(ticket_price)(raffle_desc)(raffleid))
    };

    typedef eosio::multi_index<N(raffles), raffles,
                               indexed_by<N(prize), const_mem_fun<raffles, uint64_t, &raffles::by_prize>>,
                               indexed_by<N(ticket_price), const_mem_fun<raffles, key256, &raffles::by_ticket_price>>>
        rafflestable;

    //@abi table numbers i64
    struct numbers
    {
        uint64_t id;
        uint64_t raffle_id;
        string colored_name;
        account_name owner;

        uint64_t primary_key() const { return id; }

        EOSLIB_SERIALIZE(numbers, (id)(raffle_id)(colored_name)(owner))
    };

    typedef eosio::multi_index<N(numbers), numbers> numberstable;


    raffletable raffles;
    numberstable numbers;
    account_index accounts;

    void pay_and_clean(const game &g, const offer &winner_offer,
                       const offer &loser_offer)
    {

    }
};

EOSIO_ABI(raffle, (createraffle)(buyticket)(raffle))
