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
| [a_binary_schema_is_a_constant](a_binary_schema_is_a_constant/) | 1 162 | 345 955 | 544 | 0.00 | 2.14 |
| [a_counted_loop_of_calls_to_one_singleton_takes_its_lock_once](a_counted_loop_of_calls_to_one_singleton_takes_its_lock_once/) | 15 756 | 284 999 | 2 962 | 0.06 | 5.32 |
| [a_crashs_report_is_kept_out_of_the_way](a_crashs_report_is_kept_out_of_the_way/) | 13 449 | 13 097 | 9 655 | 1.03 | 1.39 |
| [a_decimal_literal_beside_a_float_is_a_float](a_decimal_literal_beside_a_float_is_a_float/) | 31 528 | 504 044 | 15 874 | 0.06 | 1.99 |
| [a_deep_copy_is_written_per_class_with_a_table_only_where_a_graph_needs_one](a_deep_copy_is_written_per_class_with_a_table_only_where_a_graph_needs_one/) | 29 895 | 73 017 | 981 | 0.41 | 30.47 |
| [a_dictionary_hashes_a_key_once_cheaply](a_dictionary_hashes_a_key_once_cheaply/) | 46 240 | 80 303 | 25 360 | 0.58 | 1.82 |
| [a_dictionary_keyed_by_numbers_hashes_the_numbers](a_dictionary_keyed_by_numbers_hashes_the_numbers/) | 26 768 | 14 878 | 13 176 | 1.80 | 2.03 |
| [a_dictionary_written_out_and_only_read_by_literal_keys_is_folded](a_dictionary_written_out_and_only_read_by_literal_keys_is_folded/) | 13 027 | 316 565 | 5 538 | 0.04 | 2.35 |
| [a_foreign_name_is_never_copied](a_foreign_name_is_never_copied/) | not timed | not timed | not timed | | |
| [a_function_taking_a_type_is_compiled_per_class](a_function_taking_a_type_is_compiled_per_class/) | 50 759 | 50 545 | 50 456 | 1.00 | 1.01 |
| [a_function_value_describes_its_arguments_when_asked](a_function_value_describes_its_arguments_when_asked/) | 1 186 | 1 205 | 1 193 | 0.98 | 0.99 |
| [a_list_held_only_by_another_list_lives_in_its_slot](a_list_held_only_by_another_list_lives_in_its_slot/) | 124 420 | 231 975 | 34 804 | 0.54 | 3.57 |
| [a_list_item_read_only_to_test_it_is_not_counted](a_list_item_read_only_to_test_it_is_not_counted/) | 53 294 | 61 995 | 11 203 | 0.86 | 4.76 |
| [a_list_of_lists_filled_again_keeps_each_lists_room](a_list_of_lists_filled_again_keeps_each_lists_room/) | 100 933 | 290 814 | 6 298 | 0.35 | 16.03 |
| [a_lists_templates_read_its_elements_without_counting_them](a_lists_templates_read_its_elements_without_counting_them/) | 20 222 | 28 636 | 2 264 | 0.71 | 8.93 |
| [a_local_list_of_known_size_lives_in_the_frame](a_local_list_of_known_size_lives_in_the_frame/) | 23 071 | 22 965 | 22 502 | 1.00 | 1.03 |
| [a_loop_over_a_list_of_different_classes_runs_them_at_once](a_loop_over_a_list_of_different_classes_runs_them_at_once/) | 85 677 | 265 770 | 91 437 | 0.32 | 0.94 |
| [a_loop_over_plain_values_reads_its_count_once_and_its_items_unchecked](a_loop_over_plain_values_reads_its_count_once_and_its_items_unchecked/) | 29 106 | 142 109 | 15 823 | 0.20 | 1.84 |
| [a_loop_whose_passes_write_only_their_own_item_runs_in_bands](a_loop_whose_passes_write_only_their_own_item_runs_in_bands/) | 32 515 | 759 531 | 31 364 | 0.04 | 1.04 |
| [a_number_joined_into_text_is_written_in_place](a_number_joined_into_text_is_written_in_place/) | 70 870 | 306 942 | 15 066 | 0.23 | 4.70 |
| [a_number_read_from_bytes_is_one_load](a_number_read_from_bytes_is_one_load/) | 38 328 | 51 900 | 30 936 | 0.74 | 1.24 |
| [a_numbers_bits_are_read_in_place](a_numbers_bits_are_read_in_place/) | 64 928 | 87 171 | 22 625 | 0.74 | 2.87 |
| [a_proven_divisor_is_not_checked](a_proven_divisor_is_not_checked/) | 28 669 | 28 875 | 25 785 | 0.99 | 1.11 |
| [a_proven_read_tests_only_its_bounds](a_proven_read_tests_only_its_bounds/) | 32 058 | 6 525 | 6 418 | 4.91 | 5.00 |
| [a_release_build_is_o3_with_link_time_optimisation](a_release_build_is_o3_with_link_time_optimisation/) | 30 075 | 39 776 | 39 904 | 0.76 | 0.75 |
| [a_release_is_inlined_in_every_unit](a_release_is_inlined_in_every_unit/) | not timed | not timed | not timed | | |
| [a_reload_compiles_only_the_classes_that_changed](a_reload_compiles_only_the_classes_that_changed/) | not timed | not timed | not timed | | |
| [a_row_of_borrowed_items_lives_in_the_frame](a_row_of_borrowed_items_lives_in_the_frame/) | 30 627 | 6 646 | 3 025 | 4.61 | 10.12 |
| [a_singleton_no_other_thread_reaches_takes_no_lock](a_singleton_no_other_thread_reaches_takes_no_lock/) | 17 737 | 4 644 | 3 090 | 3.82 | 5.74 |
| [a_singletons_attribute_that_never_changes_is_read_in_place](a_singletons_attribute_that_never_changes_is_read_in_place/) | 18 662 | 125 609 | 3 052 | 0.15 | 6.11 |
| [a_singletons_reading_functions_do_not_exclude_each_other](a_singletons_reading_functions_do_not_exclude_each_other/) | 18 149 | 83 917 | 4 632 | 0.22 | 3.92 |
| [a_table_filled_once_is_read_as_constants](a_table_filled_once_is_read_as_constants/) | 26 933 | 20 361 | 19 451 | 1.32 | 1.38 |
| [a_test_against_a_value_a_list_never_holds_is_decided_while_compiling](a_test_against_a_value_a_list_never_holds_is_decided_while_compiling/) | 26 751 | 37 751 | 19 433 | 0.71 | 1.38 |
| [a_variadic_list_the_callee_only_reads_lives_in_the_callers_frame](a_variadic_list_the_callee_only_reads_lives_in_the_callers_frame/) | 3 986 | 3 828 | 3 869 | 1.04 | 1.03 |
| [a_wait_in_a_frame_does_not_hold_the_frame](a_wait_in_a_frame_does_not_hold_the_frame/) | 933 233 | 1 186 557 | 928 354 | 0.79 | 1.01 |
| [a_walked_crash_lines_read_is_the_rows_read](a_walked_crash_lines_read_is_the_rows_read/) | 50 065 | 17 612 | 4 478 | 2.84 | 11.18 |
| [a_word_inflected_while_compiling](a_word_inflected_while_compiling/) | 11 153 | 265 268 | 3 235 | 0.04 | 3.45 |
| [a_write_back_of_what_the_slot_already_holds_is_not_written](a_write_back_of_what_the_slot_already_holds_is_not_written/) | 25 059 | 4 869 | 545 | 5.15 | 45.98 |
| [allocation_is_the_c_librarys_counted_only_where_read](allocation_is_the_c_librarys_counted_only_where_read/) | 15 515 | 58 727 | 2 764 | 0.26 | 5.61 |
| [an_allocator_set_after_construction_is_where_the_object_is_made](an_allocator_set_after_construction_is_where_the_object_is_made/) | 46 254 | 72 483 | 1 969 | 0.64 | 23.49 |
| [an_argument_its_caller_holds_is_passed_without_counting](an_argument_its_caller_holds_is_passed_without_counting/) | 11 960 | 34 338 | 662 | 0.35 | 18.07 |
| [an_attribute_a_call_cannot_assign_is_passed_without_counting](an_attribute_a_call_cannot_assign_is_passed_without_counting/) | 4 233 | 4 230 | 2 521 | 1.00 | 1.68 |
| [an_item_a_name_holds_from_its_list_is_not_counted](an_item_a_name_holds_from_its_list_is_not_counted/) | 14 977 | 14 582 | 1 651 | 1.03 | 9.07 |
| [an_item_passed_to_a_call_that_cannot_change_its_list_is_not_counted](an_item_passed_to_a_call_that_cannot_change_its_list_is_not_counted/) | 18 185 | 15 723 | 2 108 | 1.16 | 8.63 |
| [an_item_written_back_to_its_own_slot_is_not_written](an_item_written_back_to_its_own_slot_is_not_written/) | 37 078 | 43 310 | 3 465 | 0.86 | 10.70 |
| [an_items_storage_is_chosen_while_compiling](an_items_storage_is_chosen_while_compiling/) | 29 912 | 29 564 | 2 265 | 1.01 | 13.21 |
| [appending_to_text_in_place](appending_to_text_in_place/) | 194 | 87 742 | 106 | 0.00 | 1.83 |
| [arithmetic_is_checked_in_every_build](arithmetic_is_checked_in_every_build/) | 19 767 | 2 876 | 2 601 | 6.87 | 7.60 |
| [atomic_reference_counts_only_with_threads](atomic_reference_counts_only_with_threads/) | 15 775 | 16 825 | 2 138 | 0.94 | 7.38 |
| [boxing_only_where_a_value_travels_as_a_shape](boxing_only_where_a_value_travels_as_a_shape/) | 17 402 | 85 074 | 12 047 | 0.20 | 1.44 |
| [calls_in_a_row_run_at_once](calls_in_a_row_run_at_once/) | 33 811 | 41 775 | 22 564 | 0.81 | 1.50 |
| [concurrency_machinery_only_where_it_is_used](concurrency_machinery_only_where_it_is_used/) | 29 955 | 30 487 | 1 510 | 0.98 | 19.84 |
| [copies_that_cost_nothing](copies_that_cost_nothing/) | 32 994 | 22 011 | 16 029 | 1.50 | 2.06 |
| [counts_stay_plain_for_what_one_of_the_calls_run_at_once_counts](counts_stay_plain_for_what_one_of_the_calls_run_at_once_counts/) | 7 267 | 10 400 | 5 226 | 0.70 | 1.39 |
| [crash_text_out_of_the_binary](crash_text_out_of_the_binary/) | not timed | not timed | not timed | | |
| [deciding_conditions_at_compile_time](deciding_conditions_at_compile_time/) | 11 933 | 8 362 | 7 399 | 1.43 | 1.61 |
| [defaults_the_constructor_replaces_are_never_made](defaults_the_constructor_replaces_are_never_made/) | 5 370 | 40 390 | 336 | 0.13 | 15.98 |
| [game_maths](game_maths/) | 2 655 | 2 157 | 1 686 | 1.23 | 1.57 |
| [hidden_async_await_as_compile_time_state_machines](hidden_async_await_as_compile_time_state_machines/) | not timed | not timed | not timed | | |
| [identical_functions_are_folded_into_one](identical_functions_are_folded_into_one/) | not timed | not timed | not timed | | |
| [image_filter_over_planes](image_filter_over_planes/) | 112 806 | 231 652 | 20 538 | 0.49 | 5.49 |
| [maths_on_constants_is_worked_out_while_compiling](maths_on_constants_is_worked_out_while_compiling/) | 11 570 | 13 028 | 2 814 | 0.89 | 4.11 |
| [number_dictionary](number_dictionary/) | 41 200 | 18 571 | 11 607 | 2.22 | 3.55 |
| [objects_made_for_their_owner_are_told_apart](objects_made_for_their_owner_are_told_apart/) | 58 899 | 113 331 | 57 576 | 0.52 | 1.02 |
| [objects_of_one_class_sit_together](objects_of_one_class_sit_together/) | 10 201 | 21 470 | 2 352 | 0.48 | 4.34 |
| [objects_that_never_leave_their_function_live_in_the_frame](objects_that_never_leave_their_function_live_in_the_frame/) | 13 415 | 999 113 | 14 136 | 0.01 | 0.95 |
| [other_optimisations](other_optimisations/) | not timed | not timed | not timed | | |
| [particles](particles/) | 35 661 | 51 754 | 31 366 | 0.69 | 1.14 |
| [plain_reference_counts_where_no_thread_reaches_a_class](plain_reference_counts_where_no_thread_reaches_a_class/) | 15 407 | 24 553 | 10 697 | 0.63 | 1.44 |
| [proofs_that_survive_a_call](proofs_that_survive_a_call/) | not timed | not timed | not timed | | |
| [reading_an_address_is_one_machine_operation](reading_an_address_is_one_machine_operation/) | 17 020 | 5 455 | 3 916 | 3.12 | 4.35 |
| [reading_through_a_type_without_counting](reading_through_a_type_without_counting/) | 27 815 | 9 328 | 5 173 | 2.98 | 5.38 |
| [reads_in_a_row_overlap](reads_in_a_row_overlap/) | 15 342 | 12 627 | 11 057 | 1.22 | 1.39 |
| [records_sorted_by_one_field](records_sorted_by_one_field/) | 550 709 | 637 983 | 83 559 | 0.86 | 6.59 |
| [reflection_on_constants_folds_and_unrolls](reflection_on_constants_folds_and_unrolls/) | 12 173 | 11 489 | 2 497 | 1.06 | 4.88 |
| [reflection_symbols_and_registries_only_where_read](reflection_symbols_and_registries_only_where_read/) | not timed | not timed | not timed | | |
| [removing_many_at_once](removing_many_at_once/) | 79 648 | 23 664 | 20 304 | 3.37 | 3.92 |
| [repl_live_reload_and_debug_machinery_only_in_those_builds](repl_live_reload_and_debug_machinery_only_in_those_builds/) | not timed | not timed | not timed | | |
| [report_over_records](report_over_records/) | 308 670 | 565 770 | 21 972 | 0.55 | 14.05 |
| [short_symbols_are_inline_text](short_symbols_are_inline_text/) | not timed | not timed | not timed | | |
| [short_text_lives_inside_the_string](short_text_lives_inside_the_string/) | 44 894 | 244 734 | 16 767 | 0.18 | 2.68 |
| [singletons_a_parallel_reaches_take_a_lock](singletons_a_parallel_reaches_take_a_lock/) | 8 541 | 18 910 | 486 | 0.45 | 17.57 |
| [singletons_made_on_first_use_never_counted](singletons_made_on_first_use_never_counted/) | 15 947 | 212 008 | 5 547 | 0.08 | 2.87 |
| [singletons_that_hold_nothing_are_static_objects](singletons_that_hold_nothing_are_static_objects/) | not timed | not timed | not timed | | |
| [smaller_ones](smaller_ones/) | 25 791 | 9 692 | 8 745 | 2.66 | 2.95 |
| [sorting](sorting/) | 141 324 | 118 351 | 18 817 | 1.19 | 7.51 |
| [spreadsheet_recalculation](spreadsheet_recalculation/) | 620 120 | 598 079 | 101 337 | 1.04 | 6.12 |
| [storing_an_object_into_a_list_counts_it_only_when_it_changes_the_slot](storing_an_object_into_a_list_counts_it_only_when_it_changes_the_slot/) | 8 166 | 6 404 | 4 219 | 1.28 | 1.94 |
| [template_chains_run_as_one_loop](template_chains_run_as_one_loop/) | 11 029 | 73 512 | 1 578 | 0.15 | 6.99 |
| [text_building](text_building/) | 143 849 | 80 846 | 9 305 | 1.78 | 15.46 |
| [text_joined_in_one_piece](text_joined_in_one_piece/) | 32 982 | 96 080 | 3 289 | 0.34 | 10.03 |
| [the_c_is_compiled_in_parallel_units_and_cached](the_c_is_compiled_in_parallel_units_and_cached/) | not timed | not timed | not timed | | |
| [the_compiler_places_memory](the_compiler_places_memory/) | 15 029 | 32 399 | 8 523 | 0.46 | 1.76 |
| [the_fault_handler_is_in_every_program](the_fault_handler_is_in_every_program/) | not timed | not timed | not timed | | |
| [the_thread_pool_only_where_a_parallel_is_made](the_thread_pool_only_where_a_parallel_is_made/) | 8 878 | 7 064 | 7 055 | 1.26 | 1.26 |
| [thread_safety_for_singletons_the_cheapest_safe_form](thread_safety_for_singletons_the_cheapest_safe_form/) | 34 907 | 157 826 | 389 | 0.22 | 89.74 |
| [thread_safety_for_singletons_the_rest_of_the_plan](thread_safety_for_singletons_the_rest_of_the_plan/) | 10 770 | 18 247 | 1 802 | 0.59 | 5.98 |
| [tokens_as_columns](tokens_as_columns/) | 99 540 | 127 415 | 71 622 | 0.78 | 1.39 |
| [tree_shaking_the_generated_c](tree_shaking_the_generated_c/) | not timed | not timed | not timed | | |
| [vector_maths](vector_maths/) | 80 130 | 63 438 | 47 199 | 1.26 | 1.70 |
| [what_a_hot_reload_build_carries_so_its_objects_can_move](what_a_hot_reload_build_carries_so_its_objects_can_move/) | not timed | not timed | not timed | | |
| [while_no_task_runs_a_singletons_lock_is_skipped](while_no_task_runs_a_singletons_lock_is_skipped/) | 5 270 | 52 234 | 2 219 | 0.10 | 2.37 |
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
