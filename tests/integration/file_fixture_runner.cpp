// Test-only runner: caller provides bytes as hex; the guest sees only a virtual path.
#include "loader/elf64.h"
#include "runtime/linux_process.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
int main(int argc,char** argv){
 if(argc!=3)return 100;std::string hex=argv[2];if(hex.size()>192 || hex.size()%2)return 100;
 auto digit=[](char c)->int{if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;return -1;};
 std::vector<std::uint8_t> content;for(std::size_t i=0;i<hex.size();i+=2){int a=digit(hex[i]),b=digit(hex[i+1]);if(a<0||b<0)return 100;content.push_back((a<<4)|b);}
 std::ifstream file(argv[1],std::ios::binary);if(!file)return 101;std::vector<std::uint8_t> elf((std::istreambuf_iterator<char>(file)),{});if(elf.size()>1024*1024)return 101;
 constexpr std::uint64_t size=1ULL<<44,stack=65536;nightfall::Machine m(size,nightfall::SparsePages{4096});
 if(nightfall::loader::load_elf64(elf,m,size-stack,stack)!=nightfall::loader::ElfStatus::ok)return 102;
 const std::array<std::string,2> args{"fixture","/test/input"};if(!nightfall::runtime::prepare_linux_stack(m,size-stack,stack,args))return 103;
 nightfall::runtime::LinuxProcess p(m);if(!p.add_test_file("/test/input",content))return 104;
 const auto report=p.run(2000);if(report.result!=nightfall::runtime::ProcessResult::exited)return 105;
#ifdef _WIN32
 if(_setmode(_fileno(stdout),_O_BINARY)==-1)return 106;
#endif
 const auto& out=p.stdout_bytes();if(!out.empty())std::cout.write(reinterpret_cast<const char*>(out.data()),out.size());if(!std::cout)return 106;return report.exit_code;
}
