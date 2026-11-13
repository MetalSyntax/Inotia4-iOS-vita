// Generado por tools/gen_cxx.py: imports C++ del juego -> libstdc++ de VitaSDK.
// No editar a mano (volver a correr gen_cxx.py con el report.txt de ios_prep.py).
#include <string.h>

#include "imports.h"

extern char cxx_0 __asm__("_ZNKSs5c_strEv"); // std::basic_string<char, std::char_traits<char>, std::allocator<char> >::c_str() const
extern char cxx_1 __asm__("_ZNKSs7compareEPKc"); // std::basic_string<char, std::char_traits<char>, std::allocator<char> >::compare(char const*) const
extern char cxx_2 __asm__("_ZNSsC1EPKcRKSaIcE"); // std::basic_string<char, std::char_traits<char>, std::allocator<char> >::basic_string(char const*, std::allocator<char> const&)
extern char cxx_3 __asm__("_ZNSsC1ERKSs"); // std::basic_string<char, std::char_traits<char>, std::allocator<char> >::basic_string(std::basic_string<char, std::char_traits<char>, std::allocator<char> > const&)
extern char cxx_4 __asm__("_ZNSsC1Ev"); // std::basic_string<char, std::char_traits<char>, std::allocator<char> >::basic_string()
extern char cxx_5 __asm__("_ZNSsD1Ev"); // std::basic_string<char, std::char_traits<char>, std::allocator<char> >::~basic_string()
extern char cxx_6 __asm__("_ZNSsD2Ev"); // std::basic_string<char, std::char_traits<char>, std::allocator<char> >::~basic_string()
extern char cxx_7 __asm__("_ZNSsaSEPKc"); // std::basic_string<char, std::char_traits<char>, std::allocator<char> >::operator=(char const*)
extern char cxx_8 __asm__("_ZSt9terminatev"); // std::terminate()
extern char cxx_9 __asm__("_ZTVN10__cxxabiv117__class_type_infoE"); // vtable for __cxxabiv1::__class_type_info
extern char cxx_10 __asm__("_ZTVN10__cxxabiv120__si_class_type_infoE"); // vtable for __cxxabiv1::__si_class_type_info
extern char cxx_11 __asm__("_ZdlPv"); // operator delete(void*)
extern char cxx_12 __asm__("_Znwj"); // operator new(unsigned long)

const import_entry cxx_imports[] = {
    {"__ZNKSs5c_strEv", (uintptr_t)&cxx_0, 0},
    {"__ZNKSs7compareEPKc", (uintptr_t)&cxx_1, 0},
    {"__ZNSsC1EPKcRKSaIcE", (uintptr_t)&cxx_2, 0},
    {"__ZNSsC1ERKSs", (uintptr_t)&cxx_3, 0},
    {"__ZNSsC1Ev", (uintptr_t)&cxx_4, 0},
    {"__ZNSsD1Ev", (uintptr_t)&cxx_5, 0},
    {"__ZNSsD2Ev", (uintptr_t)&cxx_6, 0},
    {"__ZNSsaSEPKc", (uintptr_t)&cxx_7, 0},
    {"__ZSt9terminatev", (uintptr_t)&cxx_8, 0},
    {"__ZTVN10__cxxabiv117__class_type_infoE", (uintptr_t)&cxx_9, 1},
    {"__ZTVN10__cxxabiv120__si_class_type_infoE", (uintptr_t)&cxx_10, 1},
    {"__ZdlPv", (uintptr_t)&cxx_11, 0},
    {"__Znwm", (uintptr_t)&cxx_12, 0},
};
TABLE_END(cxx)

// Lo que no tiene equivalente queda en el log como "unimpl".
const import_entry *cxx_import(const char *name) { (void)name; return NULL; }
