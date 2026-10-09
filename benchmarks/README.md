# Benchmarks

Every benchmark here compares Spite with C; none compares Spite only with Spite. Each folder is one case: one small
program in Spite, the same program in naive C and in expert C, and the C the compiler writes for it, so you can see
what the compiler does to a plain program and what it is worth against the C a person would write. Most cases are
named after a section of [docs/optimizations.md](../docs/optimizations.md), in snake_case, and show that one
optimisation; a few are whole programs that exercise several at once (below).

## Every benchmark against C

One row per case, linking to its folder. Times are the best of seven runs in microseconds: Spite built
`--optimized`, both C programs built with `clang -O2`. The two ratios are Spite's time divided by each C program's:
below 1.00 Spite is faster, above it slower. `bash benchmarks/run.sh` rewrites this table whenever a case is timed
again, so it always shows the latest measurement of every case.

<!-- summary -->
| case | Spite µs | naive C µs | expert C µs | Spite / naive C | Spite / expert C |
|---|---|---|---|---|---|
| [a_binary_schema_is_a_constant](a_binary_schema_is_a_constant/) | 2 229 | 459 504 | 607 | 0.00 | 3.67 |
| [a_counted_loop_of_calls_to_one_singleton_takes_its_lock_once](a_counted_loop_of_calls_to_one_singleton_takes_its_lock_once/) | 71 587 | 334 091 | 3 084 | 0.21 | 23.21 |
| [a_crashs_report_is_kept_out_of_the_way](a_crashs_report_is_kept_out_of_the_way/) | 22 791 | 14 668 | 12 454 | 1.55 | 1.83 |
| [a_decimal_literal_beside_a_float_is_a_float](a_decimal_literal_beside_a_float_is_a_float/) | 34 101 | 546 482 | 17 196 | 0.06 | 1.98 |
| [a_deep_copy_is_written_per_class_with_a_table_only_where_a_graph_needs_one](a_deep_copy_is_written_per_class_with_a_table_only_where_a_graph_needs_one/) | 66 230 | 113 274 | 1 579 | 0.58 | 41.94 |
| [a_dictionary_hashes_a_key_once_cheaply](a_dictionary_hashes_a_key_once_cheaply/) | 69 590 | 140 962 | 41 508 | 0.49 | 1.68 |
| [a_dictionary_keyed_by_numbers_hashes_the_numbers](a_dictionary_keyed_by_numbers_hashes_the_numbers/) | 40 309 | 17 468 | 16 373 | 2.31 | 2.46 |
| [a_dictionary_written_out_and_only_read_by_literal_keys_is_folded](a_dictionary_written_out_and_only_read_by_literal_keys_is_folded/) | 14 277 | 433 339 | 5 983 | 0.03 | 2.39 |
| [a_foreign_name_is_never_copied](a_foreign_name_is_never_copied/) | not timed | not timed | not timed | | |
| [a_function_taking_a_type_is_compiled_per_class](a_function_taking_a_type_is_compiled_per_class/) | 62 760 | 60 717 | 60 963 | 1.03 | 1.03 |
| [a_function_value_describes_its_arguments_when_asked](a_function_value_describes_its_arguments_when_asked/) | 1 210 | 1 607 | 1 450 | 0.75 | 0.83 |
| [a_list_item_read_only_to_test_it_is_not_counted](a_list_item_read_only_to_test_it_is_not_counted/) | 58 596 | 70 666 | 12 480 | 0.83 | 4.70 |
| [a_lists_templates_read_its_elements_without_counting_them](a_lists_templates_read_its_elements_without_counting_them/) | 29 290 | 39 866 | 2 570 | 0.73 | 11.40 |
| [a_local_list_of_known_size_lives_in_the_frame](a_local_list_of_known_size_lives_in_the_frame/) | 29 150 | 26 885 | 24 939 | 1.08 | 1.17 |
| [a_loop_over_a_list_of_different_classes_runs_them_at_once](a_loop_over_a_list_of_different_classes_runs_them_at_once/) | 90 568 | 284 023 | 96 497 | 0.32 | 0.94 |
| [a_loop_over_plain_values_reads_its_count_once_and_its_items_unchecked](a_loop_over_plain_values_reads_its_count_once_and_its_items_unchecked/) | 39 530 | 159 928 | 17 720 | 0.25 | 2.23 |
| [a_loop_whose_passes_write_only_their_own_item_runs_in_bands](a_loop_whose_passes_write_only_their_own_item_runs_in_bands/) | 51 271 | 819 981 | 48 868 | 0.06 | 1.05 |
| [a_number_joined_into_text_is_written_in_place](a_number_joined_into_text_is_written_in_place/) | 84 339 | 350 714 | 16 825 | 0.24 | 5.01 |
| [a_number_read_from_bytes_is_one_load](a_number_read_from_bytes_is_one_load/) | 41 143 | 54 481 | 33 020 | 0.76 | 1.25 |
| [a_numbers_bits_are_read_in_place](a_numbers_bits_are_read_in_place/) | 73 162 | 95 957 | 24 664 | 0.76 | 2.97 |
| [a_proven_divisor_is_not_checked](a_proven_divisor_is_not_checked/) | 31 204 | 32 479 | 28 691 | 0.96 | 1.09 |
| [a_proven_read_tests_only_its_bounds](a_proven_read_tests_only_its_bounds/) | 36 851 | 7 545 | 7 613 | 4.88 | 4.84 |
| [a_release_build_is_o3_with_link_time_optimisation](a_release_build_is_o3_with_link_time_optimisation/) | 31 988 | 44 651 | 44 501 | 0.72 | 0.72 |
| [a_release_is_inlined_in_every_unit](a_release_is_inlined_in_every_unit/) | not timed | not timed | not timed | | |
| [a_reload_compiles_only_the_classes_that_changed](a_reload_compiles_only_the_classes_that_changed/) | not timed | not timed | not timed | | |
| [a_row_of_borrowed_items_lives_in_the_frame](a_row_of_borrowed_items_lives_in_the_frame/) | 34 176 | 9 447 | 4 240 | 3.62 | 8.06 |
| [a_singleton_no_other_thread_reaches_takes_no_lock](a_singleton_no_other_thread_reaches_takes_no_lock/) | 21 943 | 4 926 | 3 253 | 4.45 | 6.75 |
| [a_singletons_attribute_that_never_changes_is_read_in_place](a_singletons_attribute_that_never_changes_is_read_in_place/) | 32 297 | 179 131 | 3 533 | 0.18 | 9.14 |
| [a_singletons_reading_functions_do_not_exclude_each_other](a_singletons_reading_functions_do_not_exclude_each_other/) | 17 641 | 105 106 | 5 547 | 0.17 | 3.18 |
| [a_test_against_a_value_a_list_never_holds_is_decided_while_compiling](a_test_against_a_value_a_list_never_holds_is_decided_while_compiling/) | 28 970 | 41 692 | 21 221 | 0.69 | 1.37 |
| [a_variadic_list_the_callee_only_reads_lives_in_the_callers_frame](a_variadic_list_the_callee_only_reads_lives_in_the_callers_frame/) | 4 592 | 6 092 | 4 277 | 0.75 | 1.07 |
| [a_walked_crash_lines_read_is_the_rows_read](a_walked_crash_lines_read_is_the_rows_read/) | 64 399 | 31 467 | 5 189 | 2.05 | 12.41 |
| [a_word_inflected_while_compiling](a_word_inflected_while_compiling/) | 12 376 | 303 760 | 3 410 | 0.04 | 3.63 |
| [allocation_is_the_c_librarys_counted_only_where_read](allocation_is_the_c_librarys_counted_only_where_read/) | 89 408 | 67 508 | 2 877 | 1.32 | 31.08 |
| [an_allocator_set_after_construction_is_where_the_object_is_made](an_allocator_set_after_construction_is_where_the_object_is_made/) | 64 161 | 82 922 | 2 059 | 0.77 | 31.16 |
| [an_argument_its_caller_holds_is_passed_without_counting](an_argument_its_caller_holds_is_passed_without_counting/) | 13 498 | 38 230 | 702 | 0.35 | 19.23 |
| [an_attribute_a_call_cannot_assign_is_passed_without_counting](an_attribute_a_call_cannot_assign_is_passed_without_counting/) | 4 727 | 4 673 | 2 791 | 1.01 | 1.69 |
| [an_item_a_name_holds_from_its_list_is_not_counted](an_item_a_name_holds_from_its_list_is_not_counted/) | 17 478 | 16 516 | 2 024 | 1.06 | 8.64 |
| [an_item_passed_to_a_call_that_cannot_change_its_list_is_not_counted](an_item_passed_to_a_call_that_cannot_change_its_list_is_not_counted/) | 22 970 | 21 764 | 2 562 | 1.06 | 8.97 |
| [an_item_written_back_to_its_own_slot_is_not_written](an_item_written_back_to_its_own_slot_is_not_written/) | 44 608 | 54 766 | 4 091 | 0.81 | 10.90 |
| [an_items_storage_is_chosen_while_compiling](an_items_storage_is_chosen_while_compiling/) | 38 758 | 65 374 | 3 007 | 0.59 | 12.89 |
| [appending_to_text_in_place](appending_to_text_in_place/) | 165 | 88 282 | 196 | 0.00 | 0.84 |
| [arithmetic_is_checked_in_every_build](arithmetic_is_checked_in_every_build/) | 21 612 | 3 371 | 3 086 | 6.41 | 7.00 |
| [atomic_reference_counts_only_with_threads](atomic_reference_counts_only_with_threads/) | 20 865 | 28 207 | 2 280 | 0.74 | 9.15 |
| [boxing_only_where_a_value_travels_as_a_shape](boxing_only_where_a_value_travels_as_a_shape/) | 20 380 | 150 885 | 12 572 | 0.14 | 1.62 |
| [calls_in_a_row_run_at_once](calls_in_a_row_run_at_once/) | 38 672 | 46 390 | 24 947 | 0.83 | 1.55 |
| [concurrency_machinery_only_where_it_is_used](concurrency_machinery_only_where_it_is_used/) | 37 798 | 39 005 | 1 675 | 0.97 | 22.57 |
| [copies_that_cost_nothing](copies_that_cost_nothing/) | 36 787 | 24 689 | 17 688 | 1.49 | 2.08 |
| [crash_text_out_of_the_binary](crash_text_out_of_the_binary/) | not timed | not timed | not timed | | |
| [deciding_conditions_at_compile_time](deciding_conditions_at_compile_time/) | 12 745 | 9 466 | 8 435 | 1.35 | 1.51 |
| [defaults_the_constructor_replaces_are_never_made](defaults_the_constructor_replaces_are_never_made/) | 29 679 | 56 403 | 355 | 0.53 | 83.60 |
| [game_maths](game_maths/) | 3 082 | 2 493 | 1 947 | 1.24 | 1.58 |
| [hidden_async_await_as_compile_time_state_machines](hidden_async_await_as_compile_time_state_machines/) | not timed | not timed | not timed | | |
| [identical_functions_are_folded_into_one](identical_functions_are_folded_into_one/) | not timed | not timed | not timed | | |
| [image_filter_over_planes](image_filter_over_planes/) | 120 602 | 263 092 | 24 839 | 0.46 | 4.86 |
| [maths_on_constants_is_worked_out_while_compiling](maths_on_constants_is_worked_out_while_compiling/) | 15 243 | 16 829 | 3 285 | 0.91 | 4.64 |
| [number_dictionary](number_dictionary/) | 78 997 | 26 543 | 14 231 | 2.98 | 5.55 |
| [objects_of_one_class_sit_together](objects_of_one_class_sit_together/) | 13 272 | 27 335 | 2 756 | 0.49 | 4.82 |
| [objects_that_never_leave_their_function_live_in_the_frame](objects_that_never_leave_their_function_live_in_the_frame/) | 15 277 | 1 132 776 | 15 248 | 0.01 | 1.00 |
| [other_optimisations](other_optimisations/) | not timed | not timed | not timed | | |
| [particles](particles/) | 52 378 | 71 996 | 45 967 | 0.73 | 1.14 |
| [plain_reference_counts_where_no_thread_reaches_a_class](plain_reference_counts_where_no_thread_reaches_a_class/) | 21 338 | 34 321 | 11 434 | 0.62 | 1.87 |
| [proofs_that_survive_a_call](proofs_that_survive_a_call/) | not timed | not timed | not timed | | |
| [reading_an_address_is_one_machine_operation](reading_an_address_is_one_machine_operation/) | 21 039 | 6 262 | 6 399 | 3.36 | 3.29 |
| [reading_through_a_type_without_counting](reading_through_a_type_without_counting/) | 29 834 | 10 669 | 5 731 | 2.80 | 5.21 |
| [reads_in_a_row_overlap](reads_in_a_row_overlap/) | 35 659 | 27 668 | 22 527 | 1.29 | 1.58 |
| [records_sorted_by_one_field](records_sorted_by_one_field/) | 591 668 | 693 851 | 89 569 | 0.85 | 6.61 |
| [reflection_on_constants_folds_and_unrolls](reflection_on_constants_folds_and_unrolls/) | 13 980 | 14 561 | 2 944 | 0.96 | 4.75 |
| [reflection_symbols_and_registries_only_where_read](reflection_symbols_and_registries_only_where_read/) | not timed | not timed | not timed | | |
| [removing_many_at_once](removing_many_at_once/) | 85 563 | 25 034 | 21 632 | 3.42 | 3.96 |
| [repl_live_reload_and_debug_machinery_only_in_those_builds](repl_live_reload_and_debug_machinery_only_in_those_builds/) | not timed | not timed | not timed | | |
| [report_over_records](report_over_records/) | 392 621 | 755 432 | 25 313 | 0.52 | 15.51 |
| [short_symbols_are_inline_text](short_symbols_are_inline_text/) | not timed | not timed | not timed | | |
| [short_text_lives_inside_the_string](short_text_lives_inside_the_string/) | 56 239 | 351 738 | 19 692 | 0.16 | 2.86 |
| [singletons_a_parallel_reaches_take_a_lock](singletons_a_parallel_reaches_take_a_lock/) | 9 862 | 33 075 | 742 | 0.30 | 13.29 |
| [singletons_made_on_first_use_never_counted](singletons_made_on_first_use_never_counted/) | 20 693 | 253 962 | 6 271 | 0.08 | 3.30 |
| [singletons_that_hold_nothing_are_static_objects](singletons_that_hold_nothing_are_static_objects/) | not timed | not timed | not timed | | |
| [smaller_ones](smaller_ones/) | 28 994 | 11 852 | 9 616 | 2.45 | 3.02 |
| [sorting](sorting/) | 171 772 | 145 960 | 29 981 | 1.18 | 5.73 |
| [spreadsheet_recalculation](spreadsheet_recalculation/) | 821 918 | 741 232 | 119 537 | 1.11 | 6.88 |
| [storing_an_object_into_a_list_counts_it_only_when_it_changes_the_slot](storing_an_object_into_a_list_counts_it_only_when_it_changes_the_slot/) | 9 302 | 6 926 | 4 578 | 1.34 | 2.03 |
| [template_chains_run_as_one_loop](template_chains_run_as_one_loop/) | 18 997 | 113 430 | 1 669 | 0.17 | 11.38 |
| [text_building](text_building/) | 205 168 | 113 842 | 12 087 | 1.80 | 16.97 |
| [text_joined_in_one_piece](text_joined_in_one_piece/) | 39 686 | 107 552 | 3 828 | 0.37 | 10.37 |
| [the_c_is_compiled_in_parallel_units_and_cached](the_c_is_compiled_in_parallel_units_and_cached/) | not timed | not timed | not timed | | |
| [the_compiler_places_memory](the_compiler_places_memory/) | 31 150 | 68 521 | 17 859 | 0.45 | 1.74 |
| [the_fault_handler_is_in_every_program](the_fault_handler_is_in_every_program/) | not timed | not timed | not timed | | |
| [the_thread_pool_only_where_a_parallel_is_made](the_thread_pool_only_where_a_parallel_is_made/) | 18 241 | 12 624 | 12 320 | 1.44 | 1.48 |
| [thread_safety_for_singletons_the_cheapest_safe_form](thread_safety_for_singletons_the_cheapest_safe_form/) | 12 015 | 165 715 | 1 050 | 0.07 | 11.44 |
| [thread_safety_for_singletons_the_rest_of_the_plan](thread_safety_for_singletons_the_rest_of_the_plan/) | 10 998 | 20 699 | 1 947 | 0.53 | 5.65 |
| [tokens_as_columns](tokens_as_columns/) | 124 524 | 149 026 | 80 375 | 0.84 | 1.55 |
| [tree_shaking_the_generated_c](tree_shaking_the_generated_c/) | not timed | not timed | not timed | | |
| [vector_maths](vector_maths/) | 94 694 | 73 269 | 54 676 | 1.29 | 1.73 |
| [what_a_hot_reload_build_carries_so_its_objects_can_move](what_a_hot_reload_build_carries_so_its_objects_can_move/) | not timed | not timed | not timed | | |
| [while_no_task_runs_a_singletons_lock_is_skipped](while_no_task_runs_a_singletons_lock_is_skipped/) | 52 634 | 76 369 | 2 806 | 0.69 | 18.76 |
<!-- /summary -->

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
([clock.h](clock.h)) and prints `microseconds <n>` on its error output, so starting a process is not counted. In
`naive/` the entry function shows only the measuring and the printing: `var result = Benchmark(work)` runs the
case's work, a function of its own, once between two readings of the clock
([Measuring a piece of work](../docs/time.md#measuring-a-piece-of-work)), and what is not measured (filling a
list before the work, say) is a function called before that line. Five cases about the locks and counts a
`Parallel` brings read the clock twice instead, since the work `Benchmark` runs counts as code another thread may
run in a program with a `Parallel` ([time.md](../docs/time.md#measuring-a-piece-of-work)). The C programs read the clock the same way,
`int64_t start = now_nanoseconds();` before the work and `microseconds_since(start)` after it, and print the line
with `print_microseconds`.

## How to read the numbers

The two C programs are two amounts of effort. `naive.c` is what you get for the effort of writing the Spite program:
the same objects, the same steps, nothing tuned. `expert.c` is what you get for an expert's effort: the data laid
out for the loop, the loops fused, the checks dropped by hand. Each case is timed against both, so its numbers read
as developer effort against speed:

**Only the result counts.** Any form is fair as long as it prints the same answer. If `expert.c` skips work, folds it
away while compiling, or computes the answer some other way, that is not cheating: it is the target, and the compiler
must learn to reach it from the unchanged Spite program by proving the same result.

**Any method counts, the fastest wins.** Whatever can be worked out while compiling should be: a compiler that proves
the answer in advance and only prints it has won that case. What no form may do is answer something else: every
form must solve the same problem and print the same answer, and `check.sh` compares the three forms' output on every
case. The data may change completely on the way (another layout, another algorithm, no data at all), as long as the
answer does not.


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

## Archetype cases

Four cases measure grouping the items of one list by which optional parts they have (one table per set of present
parts), which the compiler does not do yet, for
[design/proposals/compiler_archetypes.md](../design/proposals/compiler_archetypes.md). Each keeps its hand forms
beside `expert.c`, every one printing the same answer: `expert_tables.c` (one table per set of parts, items moved
between tables when a part comes or goes), `expert_sparse.c` (each part in a sparse set of its own), `expert_hybrid.c`
where a part churns (the parts that never change decide the table, the churning ones are sparse), `expert_dense.c`
(columns over every item with holes for absent parts) and `expert_inline.c` (the parts inside their owner, the
smallest step from what Spite writes today); `expert.c` includes the one that is fastest at the case's defaults.
Every form takes its size, the share of items with each part and the churn as settings, and the proposal's
`sweep.sh` times them all across those:

- [shapes_with_optional_parts](shapes_with_optional_parts/): 200 000 shapes, some with a texture, some with a
  velocity, some with both; three passes that each need some of the parts. Nothing churns.
- [inventory_with_fields_set_and_cleared](inventory_with_fields_set_and_cleared/): 200 000 records with a supplier
  that never changes and a reservation set and cleared on 2 000 records a pass.
- [events_with_different_payloads](events_with_different_payloads/): a queue of a union of three payloads, filled
  and handled in order forty times.
- [entities_with_components_added_and_removed](entities_with_components_added_and_removed/): 200 000 entities with a
  velocity and health that stay and a burning component added to 2 000 a pass and removed when it runs out.

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
