/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

int main(int argument_count, char** argument_values) {
    spite_fault_install();
    spite_program_arguments = (SpiteArguments){ .count = argument_count - 1, .items = (const char**)(argument_values + 1) };
    #ifdef _WIN32
    _setmode(_fileno(stdout), _O_BINARY);
    #endif
    Launcher* spite_launcher = Launcher___allocate();
    Launcher_Launcher(spite_launcher);
    Launcher___release(spite_launcher);
    spite_singletons_destroy();
    
    
    
    if (spite_foreign_library_1_tracked) DynamicLibrary___destroy(spite_foreign_library_1_cache);
    
    
    
    return 0;
}

static void spite_fault_install(void) {
    spite_crash_chain = spite_crash_frames;
    SetUnhandledExceptionFilter(spite_fault_filter);
    AddVectoredExceptionHandler(0, spite_fault_vectored);
    spite_fault_thread();
}

static void spite_fault_thread(void) {
    ULONG reserve = 16384;
    SetThreadStackGuarantee(&reserve);
}

{(const void*)&Naive___init, "-\t-", "Naive___init", 0},
{(const void*)&Naive___allocate, "-\t-", "Naive___allocate", 0},
{(const void*)&Naive___release, "-\t-", "Naive___release", 0},
{(const void*)&Naive___free, "-\t-", "Naive___free", 0},
{(const void*)&Naive_Naive, "benchmarks/cases/the_fault_handler_is_in_every_program/naive/naive.spite\tNaive", "Naive", 3},
{(const void*)&Naive_weighted_sum___held_0, "benchmarks/cases/the_fault_handler_is_in_every_program/naive/naive.spite\tNaive", "weighted_sum", 14},

{(const void*)&main, "-\t-", "main", 0},
