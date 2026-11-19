#pragma once
#include <cstdint>
#include <string>
#include <string_view>
namespace nightfall::loader {
// Metadata qualifier IDs are not symbol NIDs and do not involve cryptography.
inline std::string sce_id_token(std::uint16_t id){
 constexpr char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-";
 std::string out;do{out.insert(out.begin(),alphabet[id%64]);id=static_cast<std::uint16_t>(id/64);}while(id);return out;
}
inline bool sce_parse_id_token(std::string_view token,std::uint16_t& result){
 constexpr std::string_view alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-";
 if(token.empty()||token.size()>3)return false;
 unsigned value=0;for(char ch:token){const auto digit=alphabet.find(ch);if(digit==alphabet.npos)return false;value=value*64+static_cast<unsigned>(digit);}
 if(value>65535||sce_id_token(static_cast<std::uint16_t>(value))!=token)return false;
 result=static_cast<std::uint16_t>(value);return true;
}
}
