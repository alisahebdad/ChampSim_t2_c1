/*
 *    Copyright 2023 The ChampSim Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ACCESS_TYPE_H
#define ACCESS_TYPE_H

#include <array>
#include <string_view>
#include <fstream>


enum class access_type : unsigned {
  LOAD = 0,
  RFO,
  PREFETCH,
  WRITE,
  TRANSLATION,
  NUM_TYPES,
};


inline std::ostream& operator<< (std::ostream& os,access_type type){
  switch(type){
    case access_type::LOAD: os << "LOAD";break;
    case access_type::RFO : os << "RFO";break;
    case access_type::PREFETCH: os << "PREFETCH";break;
    case access_type::WRITE: os << "WRITE";break;
    case access_type::TRANSLATION: os << "TRANSLATION";break;
    case access_type::NUM_TYPES: os << "NUM_TYPES";break;
    default: os << "Unknown";break;  
  }
  return os ;
}



using namespace std::literals::string_view_literals;
inline constexpr std::array<std::string_view, static_cast<std::size_t>(access_type::NUM_TYPES)> access_type_names{"LOAD"sv, "RFO"sv, "PREFETCH"sv, "WRITE"sv,
                                                                                                                  "TRANSLATION"};
#endif
