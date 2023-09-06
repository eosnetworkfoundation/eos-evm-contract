#include <eosio/eosio.hpp>
using namespace eosio;

typedef std::vector<uint8_t> bytes;

struct exec_output {
   int32_t              status;
   bytes                data;
   std::optional<bytes> context;

   EOSLIB_SERIALIZE(exec_output, (status)(data)(context));
};

struct exec_output_row {
   uint64_t    id;
   exec_output output;

   uint64_t primary_key()const {
      return id;
   }


   EOSLIB_SERIALIZE(exec_output_row, (id)(output));
};

typedef multi_index<"output"_n, exec_output_row> output_table;

CONTRACT evm_read_callback : public contract {
   public:
      using contract::contract;

      ACTION callback( const exec_output& output );
};