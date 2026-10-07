# Optimisation cases

One folder per section of [docs/optimizations.md](../../docs/optimizations.md), named after the section in
snake_case, each holding one small program in four forms so you can see what the optimisation does to it and what
it is worth:

| file | what it is |
|---|---|
| `naive/` | the plain Spite program, written the obvious way: the entry file is `naive/naive.spite`, and its other classes are beside it |
| `naive.c` | the same program written in C the way a person writes it without tuning: the same loops, the same objects allocated the same way (`malloc` per object, a `struct` per class), the same calls; no `restrict`, no columns instead of objects, no inlining by hand |
| `expert.c` | the same computation tuned by hand by someone who knows the machine: any layout, any loop, as long as it does all the work the program asks for |
| `generated.c` | only the functions of the C the compiler writes from `naive/` that show the optimisation, copied out of it by [scripts/cases/extract.sh](../../scripts/cases/extract.sh) |
| `functions.txt` | the names of those functions (or `struct <Name>`), one per line |
| `README.md` | the optimisation in two sentences, links to its section and its proof, what to look at in `generated.c`, and the timings |

All three programs print the same answer on their standard output, and each times its own work with the same clock
([clock.h](clock.h)) and prints `microseconds <n>` on its error output, so starting a process is not counted.

## How to read the numbers

The two C programs are two amounts of effort. `naive.c` is what you get for the effort of writing the Spite program:
the same objects, the same steps, nothing tuned. `expert.c` is what you get for an expert's effort: the data laid
out for the loop, the loops fused, the checks dropped by hand. Each case is timed against both, so its numbers read
as developer effort against speed:

- **Spite's time over naive C's** says what writing it plainly in Spite buys over writing it plainly in C. Below
  1.00 the compiler's optimisation did work a C programmer would have had to do by hand; above it, Spite pays for
  something C at the same effort does not (its checks, its counts).
- **Spite's time over expert C's** says how far the plain program is from the best a person can do. It is the
  number the compiler is working to bring to 1.00: every gap is work it could still do on its own.

The time ratio is Spite's time divided by the C program's, so 1.00 is as fast, 2.00 takes twice as long, and lower
is better. Where an optimisation cannot be shown as a time (tree shaking, crash text, what is worked out only while
compiling), the case still has the three programs and its README says what to compare instead: the executable's
size, the lines of C, the allocations, and why there is no time.

## Running them

```
bash scripts/cases/check.sh [case ...]      what check.sh runs for every case: compile, generated.c, same answers
bash scripts/cases/extract.sh [case ...]    write generated.c again after a compiler change
bash benchmarks/cases/run.sh [case ...]     time the three forms and write the tables below and in each case
```

`check.sh` compiles every case with the compiler it built, regenerates `generated.c` and compares it with the one
here, so a case cannot drift from the compiler, and runs the three programs once to compare their answers. It times
nothing. When a compiler change alters what a case's functions compile to, run `extract.sh` for that case and read
the difference: it is the change the optimisation section should describe. The compiler's own numbered names
(`spite_temp_9028`, `spite_site_1128`) are numbered again from 1 in `generated.c`, so a change elsewhere that only
shifts those numbers does not touch a case.

`run.sh` builds `naive/` with `--optimized` (the release build: `-O3` with link-time optimisation) and both C
programs with `clang -O2`, runs the three seven times in turn, keeps the best time of each, and writes the table of
each case's README and the summary below. The machine and the date are written beside every table. The numbers
were taken on a machine other sessions were compiling and benchmarking on at the same time, so read a few percent
either way as noise.

## Spite against C, whole programs

[benchmarks/versus_c](../versus_c/) times five whole programs (vector maths, particles, a dictionary of numbers,
text building and sorting) against a C twin each. They stay there rather than becoming cases: each exercises
several optimisations at once, so none belongs to one section of the page, and a case folder is one section; and
their `twin.c` is the plain C a programmer writes, a naive form rather than an expert one, so calling it `expert.c`
would claim a ceiling it is not.

## Summary

<!-- summary -->
| case | Spite's time over naive C's | Spite's time over expert C's |
|---|---|---|
| [a_binary_schema_is_a_constant](a_binary_schema_is_a_constant/) | not measured yet | not measured yet |
| [a_counted_loop_of_calls_to_one_singleton_takes_its_lock_once](a_counted_loop_of_calls_to_one_singleton_takes_its_lock_once/) | not measured yet | not measured yet |
| [a_dictionary_written_out_and_only_read_by_literal_keys_is_folded](a_dictionary_written_out_and_only_read_by_literal_keys_is_folded/) | not measured yet | not measured yet |
| [a_function_taking_a_type_is_compiled_per_class](a_function_taking_a_type_is_compiled_per_class/) | not measured yet | not measured yet |
| [a_list_item_read_only_to_test_it_is_not_counted](a_list_item_read_only_to_test_it_is_not_counted/) | not measured yet | not measured yet |
| [a_lists_templates_read_its_elements_without_counting_them](a_lists_templates_read_its_elements_without_counting_them/) | not measured yet | not measured yet |
| [a_local_list_of_known_size_lives_in_the_frame](a_local_list_of_known_size_lives_in_the_frame/) | not measured yet | not measured yet |
| [a_loop_over_plain_values_reads_its_count_once_and_its_items_unchecked](a_loop_over_plain_values_reads_its_count_once_and_its_items_unchecked/) | not measured yet | not measured yet |
| [a_number_joined_into_text_is_written_in_place](a_number_joined_into_text_is_written_in_place/) | not measured yet | not measured yet |
| [a_proven_read_tests_only_its_bounds](a_proven_read_tests_only_its_bounds/) | not measured yet | not measured yet |
| [a_release_build_is_o3_with_link_time_optimisation](a_release_build_is_o3_with_link_time_optimisation/) | not measured yet | not measured yet |
| [a_release_is_inlined_in_every_unit](a_release_is_inlined_in_every_unit/) | not measured yet | not measured yet |
| [a_singletons_reading_functions_do_not_exclude_each_other](a_singletons_reading_functions_do_not_exclude_each_other/) | not measured yet | not measured yet |
| [a_variadic_list_the_callee_only_reads_lives_in_the_callers_frame](a_variadic_list_the_callee_only_reads_lives_in_the_callers_frame/) | not measured yet | not measured yet |
| [a_word_inflected_while_compiling](a_word_inflected_while_compiling/) | not measured yet | not measured yet |
| [allocation_is_the_c_librarys_counted_only_where_read](allocation_is_the_c_librarys_counted_only_where_read/) | not measured yet | not measured yet |
| [an_allocator_set_after_construction_is_where_the_object_is_made](an_allocator_set_after_construction_is_where_the_object_is_made/) | not measured yet | not measured yet |
| [an_item_a_name_holds_from_its_list_is_not_counted](an_item_a_name_holds_from_its_list_is_not_counted/) | not measured yet | not measured yet |
| [an_item_written_back_to_its_own_slot_is_not_written](an_item_written_back_to_its_own_slot_is_not_written/) | not measured yet | not measured yet |
| [appending_to_text_in_place](appending_to_text_in_place/) | not measured yet | not measured yet |
| [calls_in_a_row_run_at_once](calls_in_a_row_run_at_once/) | not measured yet | not measured yet |
| [copies_that_cost_nothing](copies_that_cost_nothing/) | not measured yet | not measured yet |
| [deciding_conditions_at_compile_time](deciding_conditions_at_compile_time/) | not measured yet | not measured yet |
| [defaults_the_constructor_replaces_are_never_made](defaults_the_constructor_replaces_are_never_made/) | not measured yet | not measured yet |
| [identical_functions_are_folded_into_one](identical_functions_are_folded_into_one/) | not measured yet | not measured yet |
| [maths_on_constants_is_worked_out_while_compiling](maths_on_constants_is_worked_out_while_compiling/) | not measured yet | not measured yet |
| [objects_of_one_class_sit_together](objects_of_one_class_sit_together/) | not measured yet | not measured yet |
| [objects_that_never_leave_their_function_live_in_the_frame](objects_that_never_leave_their_function_live_in_the_frame/) | not measured yet | not measured yet |
| [plain_reference_counts_where_no_thread_reaches_a_class](plain_reference_counts_where_no_thread_reaches_a_class/) | not measured yet | not measured yet |
| [reading_an_address_is_one_machine_operation](reading_an_address_is_one_machine_operation/) | not measured yet | not measured yet |
| [reflection_on_constants_folds_and_unrolls](reflection_on_constants_folds_and_unrolls/) | not measured yet | not measured yet |
| [reflection_symbols_and_registries_only_where_read](reflection_symbols_and_registries_only_where_read/) | not measured yet | not measured yet |
| [smaller_ones](smaller_ones/) | not measured yet | not measured yet |
| [template_chains_run_as_one_loop](template_chains_run_as_one_loop/) | not measured yet | not measured yet |
| [text_joined_in_one_piece](text_joined_in_one_piece/) | not measured yet | not measured yet |
| [the_compiler_places_memory](the_compiler_places_memory/) | not measured yet | not measured yet |
| [tree_shaking_the_generated_c](tree_shaking_the_generated_c/) | not measured yet | not measured yet |
<!-- /summary -->
