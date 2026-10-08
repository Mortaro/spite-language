# Benchmarks

Every benchmark here compares Spite with C; none compares Spite only with Spite. Each folder is one case: one small
program in Spite, the same program in naive C and in expert C, and the C the compiler writes for it, so you can see
what the compiler does to a plain program and what it is worth against the C a person would write. Most cases are
named after a section of [docs/optimizations.md](../docs/optimizations.md), in snake_case, and show that one
optimisation; a few are whole programs that exercise several at once (below).

| file | what it is |
|---|---|
| `naive/` | the plain Spite program, written the obvious way: the entry file is `naive/naive.spite`, and its other classes are beside it |
| `naive.c` | the same program written in C the way a person writes it without tuning: the same loops, the same objects allocated the same way (`malloc` per object, a `struct` per class), the same calls; no `restrict`, no columns instead of objects, no inlining by hand |
| `expert.c` | the same computation tuned by hand by someone who knows the machine: any layout, any loop, as long as it does all the work the program asks for |
| `generated.c` | all of the C the compiler writes from `naive/`, as an `--optimized` build for Windows writes it: tree-shaken, so a few thousand lines for a small program, with the compiler's own numbered names numbered again from 1 |
| `highlights.c` | only the functions of that C that show the optimisation, copied out of it by [scripts/cases/extract.sh](../scripts/cases/extract.sh) |
| `functions.txt` | the names of those functions (or `struct <Name>`), one per line |
| `README.md` | what the case shows, links to its section and its proof, what to look at in `highlights.c`, and the timings with both ratios |

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
is better (0.00 is less than a two-hundredth of the time). A ratio far above 1.00 against naive C is worth reading
in the case's README: in several, clang deletes the naive program's allocations itself, and in others the case
found work the compiler still does that it could leave out. Where an optimisation cannot be shown as a time (tree
shaking, crash text, what is worked out only while compiling), the case still has the three programs and its README
says what to compare instead: the executable's size, the lines of C, the allocations, and why there is no time.

**Measure only production builds.** REPL and live-reload builds are slower on purpose (every function in a slot,
breakpoints, live inspection), and a `--debug-memory` build counts every allocation, so a time taken from any of
them describes the tooling, not the program ([docs/compiler.md](../docs/compiler.md#measure-only-a-production-build)).

## Whole programs

Five cases are whole programs, each of which leans on several optimisations at once, so they belong to no one
section of the page. Their `naive.c` is the C a C programmer writes for the same work, a plain struct held by value
where the Spite has a class of numbers; their `expert.c` is that program tuned:

- [vector_maths](vector_maths/): 5 million steps of `scaled`, `+`, `cross`, `normalized` and `dot` on `Vector3`.
- [particles](particles/): 100 000 particles in a `Vector<Particle>`, 300 ticks of `each_step()`.
- [number_dictionary](number_dictionary/): 500 000 integer keys and 5 million lookups.
- [text_building](text_building/): 3 million appends, and a million words joined.
- [sorting](sorting/): a quicksort of 2 million `Integer`s in a `List<Integer>`.

Two more cases measure parts of the library against C: [game_maths](game_maths/), the library's `Vector3`,
`Quaternion` and `Matrix4` against the same passes on plain structs, and
[removing_many_at_once](removing_many_at_once/), `remove_where` on an `Items` and a `List`
([collections.md](../docs/collections.md#removing-many-at-once)).

What the compiler costs to run is not a benchmark of Spite against C, so it is measured by scripts beside the
compiler's other tools, and the numbers are kept with the case they belong to: `bash scripts/build_times.sh` times
builds from one C file and from translation units
([the_c_is_compiled_in_parallel_units_and_cached](the_c_is_compiled_in_parallel_units_and_cached/#compile-time-at-scale)),
`bash scripts/executable_size.sh` prints executables' sizes and functions
([identical_functions_are_folded_into_one](identical_functions_are_folded_into_one/#executable-size)), and each
optimisation level's speed is in
[a_release_build_is_o3_with_link_time_optimisation](a_release_build_is_o3_with_link_time_optimisation/#each-optimisation-level).

## Data-oriented cases

Five cases measure layouts the compiler does not choose yet, in programs that are not games, for
[design/proposals/data_oriented_layout.md](../design/proposals/data_oriented_layout.md). Each has two more hand
forms beside `expert.c`, where the best layout is not obvious: `expert_aos.c` keeps the records inline in one array
of structs and `expert_soa.c` keeps them as columns, both with `naive.c`'s loops, while `expert.c` is the best by
hand. Every form takes its size as a setting (`--records=N` and the like), and the proposal's `cases.sh` times them
all at five sizes:

- [report_over_records](report_over_records/): two million sales of eight fields, summed by one field, five fields
  in 64 filtered passes, and all eight.
- [tokens_as_columns](tokens_as_columns/): a text of a million and a half pieces tokenised into a list of tokens,
  then counted and summed.
- [image_filter_over_planes](image_filter_over_planes/): 2048 by 2048 pixels brightened, given contrast and
  measured; interleaved channels against planes.
- [spreadsheet_recalculation](spreadsheet_recalculation/): a million cells recalculated twenty times from two
  gathered operands each.
- [records_sorted_by_one_field](records_sorted_by_one_field/): two million orders sorted by time and walked twice.

## Running them

```
bash scripts/cases/check.sh [case ...]      what check.sh runs for every case: compile, generated.c and highlights.c, same answers
bash scripts/cases/extract.sh [case ...]    write generated.c and highlights.c again after a compiler change
bash benchmarks/run.sh [case ...]           time the three forms and write the tables below and in each case
```

`check.sh` compiles every case with the compiler it built, regenerates `generated.c` and `highlights.c` and compares
them with the ones here, so a case cannot drift from the compiler, and runs the three programs once to compare their
answers. It times nothing. When a compiler change alters what a case compiles to, run `extract.sh` and read the
difference: in `highlights.c` it is the change the optimisation section should describe. `extract.sh` writes a file
only when its content changes, so a case whose C did not change is not touched. The compiler's own numbered names
(`spite_temp_9028`, `spite_site_1128`) are numbered again from 1 in each file, so a change elsewhere that only shifts
those numbers does not touch a case. Both files hold the C for Windows (`--target-operating-system=windows`), so they
are the same on every machine; on another system `check.sh` builds the program it runs from that system's C.

`run.sh` builds `naive/` with `--optimized` (the release build: `-O3` with link-time optimisation) and both C
programs with `clang -O2`, runs the three seven times in turn, keeps the best time of each, and writes the table of
each case's README and the summary below, each only when its numbers change. The machine and the date are written
beside every table. The numbers were taken on a machine other sessions were compiling and benchmarking on at the
same time, so read a few percent either way as noise.

## Summary

<!-- summary -->
| case | Spite's time over naive C's | Spite's time over expert C's |
|---|---|---|
| [a_binary_schema_is_a_constant](a_binary_schema_is_a_constant/) | 0.00 | 3.67 |
| [a_counted_loop_of_calls_to_one_singleton_takes_its_lock_once](a_counted_loop_of_calls_to_one_singleton_takes_its_lock_once/) | 0.21 | 23.21 |
| [a_crashs_report_is_kept_out_of_the_way](a_crashs_report_is_kept_out_of_the_way/) | 1.55 | 1.83 |
| [a_decimal_literal_beside_a_float_is_a_float](a_decimal_literal_beside_a_float_is_a_float/) | 0.06 | 1.98 |
| [a_deep_copy_is_written_per_class_with_a_table_only_where_a_graph_needs_one](a_deep_copy_is_written_per_class_with_a_table_only_where_a_graph_needs_one/) | 0.58 | 41.94 |
| [a_dictionary_hashes_a_key_once_cheaply](a_dictionary_hashes_a_key_once_cheaply/) | 0.49 | 1.68 |
| [a_dictionary_keyed_by_numbers_hashes_the_numbers](a_dictionary_keyed_by_numbers_hashes_the_numbers/) | 2.31 | 2.46 |
| [a_dictionary_written_out_and_only_read_by_literal_keys_is_folded](a_dictionary_written_out_and_only_read_by_literal_keys_is_folded/) | 0.03 | 2.39 |
| [a_foreign_name_is_never_copied](a_foreign_name_is_never_copied/) | not timed | not timed |
| [a_function_taking_a_type_is_compiled_per_class](a_function_taking_a_type_is_compiled_per_class/) | 1.01 | 1.01 |
| [a_function_value_describes_its_arguments_when_asked](a_function_value_describes_its_arguments_when_asked/) | 0.75 | 0.83 |
| [a_list_item_read_only_to_test_it_is_not_counted](a_list_item_read_only_to_test_it_is_not_counted/) | 0.83 | 4.70 |
| [a_lists_templates_read_its_elements_without_counting_them](a_lists_templates_read_its_elements_without_counting_them/) | 0.73 | 11.40 |
| [a_local_list_of_known_size_lives_in_the_frame](a_local_list_of_known_size_lives_in_the_frame/) | 1.08 | 1.17 |
| [a_loop_over_a_list_of_different_classes_runs_them_at_once](a_loop_over_a_list_of_different_classes_runs_them_at_once/) | 0.32 | 0.94 |
| [a_loop_over_plain_values_reads_its_count_once_and_its_items_unchecked](a_loop_over_plain_values_reads_its_count_once_and_its_items_unchecked/) | 0.25 | 2.23 |
| [a_number_joined_into_text_is_written_in_place](a_number_joined_into_text_is_written_in_place/) | 0.24 | 5.01 |
| [a_numbers_bits_are_read_in_place](a_numbers_bits_are_read_in_place/) | 0.76 | 2.97 |
| [a_proven_divisor_is_not_checked](a_proven_divisor_is_not_checked/) | 0.96 | 1.09 |
| [a_proven_read_tests_only_its_bounds](a_proven_read_tests_only_its_bounds/) | 4.88 | 4.84 |
| [a_release_build_is_o3_with_link_time_optimisation](a_release_build_is_o3_with_link_time_optimisation/) | 0.72 | 0.72 |
| [a_release_is_inlined_in_every_unit](a_release_is_inlined_in_every_unit/) | not timed | not timed |
| [a_reload_compiles_only_the_classes_that_changed](a_reload_compiles_only_the_classes_that_changed/) | not timed | not timed |
| [a_row_of_borrowed_items_lives_in_the_frame](a_row_of_borrowed_items_lives_in_the_frame/) | 3.62 | 8.06 |
| [a_singleton_no_other_thread_reaches_takes_no_lock](a_singleton_no_other_thread_reaches_takes_no_lock/) | 4.45 | 6.75 |
| [a_singletons_attribute_that_never_changes_is_read_in_place](a_singletons_attribute_that_never_changes_is_read_in_place/) | 0.18 | 9.14 |
| [a_singletons_reading_functions_do_not_exclude_each_other](a_singletons_reading_functions_do_not_exclude_each_other/) | 0.17 | 3.18 |
| [a_test_against_a_value_a_list_never_holds_is_decided_while_compiling](a_test_against_a_value_a_list_never_holds_is_decided_while_compiling/) | 0.69 | 1.37 |
| [a_variadic_list_the_callee_only_reads_lives_in_the_callers_frame](a_variadic_list_the_callee_only_reads_lives_in_the_callers_frame/) | 0.75 | 1.07 |
| [a_walked_crash_lines_read_is_the_rows_read](a_walked_crash_lines_read_is_the_rows_read/) | 2.05 | 12.41 |
| [a_word_inflected_while_compiling](a_word_inflected_while_compiling/) | 0.04 | 3.63 |
| [allocation_is_the_c_librarys_counted_only_where_read](allocation_is_the_c_librarys_counted_only_where_read/) | 1.32 | 31.08 |
| [an_allocator_set_after_construction_is_where_the_object_is_made](an_allocator_set_after_construction_is_where_the_object_is_made/) | 0.77 | 31.16 |
| [an_argument_its_caller_holds_is_passed_without_counting](an_argument_its_caller_holds_is_passed_without_counting/) | 0.35 | 19.23 |
| [an_attribute_a_call_cannot_assign_is_passed_without_counting](an_attribute_a_call_cannot_assign_is_passed_without_counting/) | 1.01 | 1.69 |
| [an_item_a_name_holds_from_its_list_is_not_counted](an_item_a_name_holds_from_its_list_is_not_counted/) | 1.06 | 8.64 |
| [an_item_passed_to_a_call_that_cannot_change_its_list_is_not_counted](an_item_passed_to_a_call_that_cannot_change_its_list_is_not_counted/) | 1.06 | 8.97 |
| [an_item_written_back_to_its_own_slot_is_not_written](an_item_written_back_to_its_own_slot_is_not_written/) | 0.81 | 10.90 |
| [an_items_storage_is_chosen_while_compiling](an_items_storage_is_chosen_while_compiling/) | 0.59 | 12.89 |
| [appending_to_text_in_place](appending_to_text_in_place/) | 0.00 | 0.84 |
| [arithmetic_is_checked_in_every_build](arithmetic_is_checked_in_every_build/) | 6.41 | 7.00 |
| [atomic_reference_counts_only_with_threads](atomic_reference_counts_only_with_threads/) | 0.74 | 9.15 |
| [boxing_only_where_a_value_travels_as_a_shape](boxing_only_where_a_value_travels_as_a_shape/) | 0.14 | 1.62 |
| [calls_in_a_row_run_at_once](calls_in_a_row_run_at_once/) | 0.83 | 1.55 |
| [concurrency_machinery_only_where_it_is_used](concurrency_machinery_only_where_it_is_used/) | 0.97 | 22.57 |
| [copies_that_cost_nothing](copies_that_cost_nothing/) | 1.49 | 2.08 |
| [crash_text_out_of_the_binary](crash_text_out_of_the_binary/) | not timed | not timed |
| [deciding_conditions_at_compile_time](deciding_conditions_at_compile_time/) | 1.35 | 1.51 |
| [defaults_the_constructor_replaces_are_never_made](defaults_the_constructor_replaces_are_never_made/) | 0.53 | 83.60 |
| [game_maths](game_maths/) | 1.24 | 1.60 |
| [hidden_async_await_as_compile_time_state_machines](hidden_async_await_as_compile_time_state_machines/) | not timed | not timed |
| [identical_functions_are_folded_into_one](identical_functions_are_folded_into_one/) | not timed | not timed |
| [image_filter_over_planes](image_filter_over_planes/) | 0.46 | 4.86 |
| [maths_on_constants_is_worked_out_while_compiling](maths_on_constants_is_worked_out_while_compiling/) | 0.91 | 4.64 |
| [number_dictionary](number_dictionary/) | 2.98 | 5.55 |
| [objects_of_one_class_sit_together](objects_of_one_class_sit_together/) | 0.49 | 4.82 |
| [objects_that_never_leave_their_function_live_in_the_frame](objects_that_never_leave_their_function_live_in_the_frame/) | 0.01 | 1.00 |
| [other_optimisations](other_optimisations/) | not timed | not timed |
| [particles](particles/) | 0.73 | 1.14 |
| [plain_reference_counts_where_no_thread_reaches_a_class](plain_reference_counts_where_no_thread_reaches_a_class/) | 0.62 | 1.87 |
| [proofs_that_survive_a_call](proofs_that_survive_a_call/) | not timed | not timed |
| [reading_an_address_is_one_machine_operation](reading_an_address_is_one_machine_operation/) | 3.36 | 3.29 |
| [reading_through_a_type_without_counting](reading_through_a_type_without_counting/) | 2.80 | 5.21 |
| [reads_in_a_row_overlap](reads_in_a_row_overlap/) | 1.41 | 1.56 |
| [records_sorted_by_one_field](records_sorted_by_one_field/) | 0.85 | 6.61 |
| [reflection_on_constants_folds_and_unrolls](reflection_on_constants_folds_and_unrolls/) | 0.96 | 4.75 |
| [reflection_symbols_and_registries_only_where_read](reflection_symbols_and_registries_only_where_read/) | not timed | not timed |
| [removing_many_at_once](removing_many_at_once/) | 3.42 | 3.96 |
| [repl_live_reload_and_debug_machinery_only_in_those_builds](repl_live_reload_and_debug_machinery_only_in_those_builds/) | not timed | not timed |
| [report_over_records](report_over_records/) | 0.52 | 15.51 |
| [short_symbols_are_inline_text](short_symbols_are_inline_text/) | not timed | not timed |
| [short_text_lives_inside_the_string](short_text_lives_inside_the_string/) | 0.16 | 2.86 |
| [singletons_a_parallel_reaches_take_a_lock](singletons_a_parallel_reaches_take_a_lock/) | 0.30 | 13.29 |
| [singletons_made_on_first_use_never_counted](singletons_made_on_first_use_never_counted/) | 0.08 | 3.30 |
| [singletons_that_hold_nothing_are_static_objects](singletons_that_hold_nothing_are_static_objects/) | not timed | not timed |
| [smaller_ones](smaller_ones/) | 2.45 | 3.02 |
| [sorting](sorting/) | 1.20 | 7.52 |
| [spreadsheet_recalculation](spreadsheet_recalculation/) | 1.11 | 6.88 |
| [template_chains_run_as_one_loop](template_chains_run_as_one_loop/) | 0.17 | 11.38 |
| [text_building](text_building/) | 1.77 | 15.48 |
| [text_joined_in_one_piece](text_joined_in_one_piece/) | 0.37 | 10.37 |
| [the_c_is_compiled_in_parallel_units_and_cached](the_c_is_compiled_in_parallel_units_and_cached/) | not timed | not timed |
| [the_compiler_places_memory](the_compiler_places_memory/) | 0.45 | 1.74 |
| [the_fault_handler_is_in_every_program](the_fault_handler_is_in_every_program/) | not timed | not timed |
| [the_thread_pool_only_where_a_parallel_is_made](the_thread_pool_only_where_a_parallel_is_made/) | 1.44 | 1.48 |
| [thread_safety_for_singletons_the_cheapest_safe_form](thread_safety_for_singletons_the_cheapest_safe_form/) | 0.07 | 11.44 |
| [thread_safety_for_singletons_the_rest_of_the_plan](thread_safety_for_singletons_the_rest_of_the_plan/) | 0.53 | 5.65 |
| [tokens_as_columns](tokens_as_columns/) | 0.84 | 1.55 |
| [tree_shaking_the_generated_c](tree_shaking_the_generated_c/) | not timed | not timed |
| [vector_maths](vector_maths/) | 1.30 | 1.70 |
| [what_a_hot_reload_build_carries_so_its_objects_can_move](what_a_hot_reload_build_carries_so_its_objects_can_move/) | not timed | not timed |
| [while_no_task_runs_a_singletons_lock_is_skipped](while_no_task_runs_a_singletons_lock_is_skipped/) | 0.69 | 18.76 |
<!-- /summary -->
