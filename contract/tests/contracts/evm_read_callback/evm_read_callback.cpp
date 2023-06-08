#include "evm_read_callback.hpp"
ACTION evm_read_callback::callback( const exec_output& output ) {
   output_table outputs(get_self(), get_self().value);
   outputs.emplace(get_self(), [&](auto& row){
      row.id = outputs.available_primary_key();
      row.output = output;
   });
}