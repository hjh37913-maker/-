#pragma once
#include "loader/static_pie.h"
#include "core/machine.h"
#include <string>
#include <vector>
namespace nightfall::runtime {
// Explicit, synthetic SysV-x86-64 guest services. These are portable primitives,
// not verified PS5 exports, an OS implementation, or a security sandbox.
enum class ServiceKind { copy, move, fill, compare, string_length };
enum class ServiceStatus { not_service, completed, memory_fault, resource_limit, unsupported };
class GuestServices {
public:
 // Must be configured before constructing bindings; addresses are guest virtual
 // trap locations, never host function pointers. Duplicate names/addresses fail.
 bool add(std::string name,std::uint64_t address,ServiceKind kind);
 std::vector<loader::ImportBinding> bindings() const;
 // Modifying/destroying this table invalidates the string_views in bindings().
 ServiceStatus dispatch(Machine& machine) const;
private:
 struct Entry {std::string name;std::uint64_t address;ServiceKind kind;};
 std::vector<Entry> entries_;
};
enum class GuestRunResult { budget_exhausted, halted, syscall_trap, invalid_instruction, memory_fault, divide_error, alignment_fault, service_limit, unsupported_service };
struct GuestRunReport { GuestRunResult result=GuestRunResult::budget_exhausted;std::size_t attempted=0;std::uint64_t stop_rip=0; };
// Each bounded service invocation costs one step, like a CPU instruction.
GuestRunReport run_with_services(Machine& machine,const GuestServices& services,std::size_t budget);
}
