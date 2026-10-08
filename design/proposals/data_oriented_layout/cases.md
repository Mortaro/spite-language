# Data-oriented cases at five sizes

Written by `bash design/proposals/data_oriented_layout/cases.sh` on 2026-10-08. Best microseconds of the
interleaved runs (seven below two million records, three above), each form's whole program as it times itself.

## report_over_records

| form | 512 | 8192 | 131072 | 2097152 | 8388608 |
|---|---|---|---|---|---|
| spite | 61 | 912 | 12936 | 424407 | 1739263 |
| naive | 75 | 956 | 22528 | 778396 | 3027077 |
| expert_aos | 40 | 563 | 10930 | 286562 | 1149726 |
| expert_soa | 46 | 923 | 12259 | 190302 | 789674 |
| expert | 6 | 129 | 1780 | 29435 | 102481 |

Phases of each C form's best run, in microseconds:

| form | 512 | 8192 | 131072 | 2097152 | 8388608 |
|---|---|---|---|---|---|
| naive | make 33 one 0 five 40 all 1 | make 391 one 32 five 516 all 16 | make 7771 one 951 five 13520 all 284 | make 109952 one 8737 five 638948 all 20758 | make 447434 one 37302 five 2453940 all 88401 |
| expert_aos | make 11 one 0 five 28 all 0 | make 101 one 3 five 446 all 13 | make 1532 one 311 five 8771 all 315 | make 23705 one 3125 five 250697 all 9033 | make 92913 one 12872 five 1024475 all 19464 |
| expert_soa | make 10 one 0 five 35 all 1 | make 183 one 3 five 711 all 24 | make 1705 one 64 five 10076 all 412 | make 26186 one 821 five 158865 all 4429 | make 95020 one 2483 five 670159 all 22010 |
| expert | make 5 fused 1 | make 109 fused 20 | make 1451 fused 328 | make 21975 fused 7459 | make 81502 fused 20978 |

## tokens_as_columns

| form | 512 | 8192 | 131072 | 2097152 | 8388608 |
|---|---|---|---|---|---|
| spite | 46 | 600 | 9160 | 152174 | 616994 |
| naive | 49 | 649 | 10590 | 190317 | 781554 |
| expert_aos | 33 | 438 | 6957 | 113157 | 459058 |
| expert_soa | 38 | 473 | 7042 | 113120 | 445096 |
| expert | 34 | 431 | 6526 | 103452 | 414856 |

Phases of each C form's best run, in microseconds:

| form | 512 | 8192 | 131072 | 2097152 | 8388608 |
|---|---|---|---|---|---|
| naive | write 20 tokenise 26 passes 2 | write 262 tokenise 332 passes 54 | write 4116 tokenise 5523 passes 950 | write 63844 tokenise 91485 passes 34987 | write 257882 tokenise 365401 passes 158270 |
| expert_aos | write 20 tokenise 11 passes 2 | write 271 tokenise 130 passes 37 | write 4097 tokenise 2027 passes 832 | write 64909 tokenise 32505 passes 15742 | write 262069 tokenise 130552 passes 66436 |
| expert_soa | write 20 tokenise 15 passes 2 | write 274 tokenise 162 passes 36 | write 4164 tokenise 2321 passes 556 | write 65024 tokenise 39023 passes 9072 | write 260863 tokenise 146878 passes 37354 |
| expert | write 19 tokenise 13 passes 1 | write 263 tokenise 151 passes 16 | write 4019 tokenise 2245 passes 261 | write 62853 tokenise 36262 passes 4336 | write 255527 tokenise 142014 passes 17314 |

## image_filter_over_planes

| form | 512 | 8192 | 131072 | 2097152 | 8388608 |
|---|---|---|---|---|---|
| spite | 24 | 253 | 3376 | 57138 | 228629 |
| naive | 24 | 347 | 6121 | 127427 | 517592 |
| expert_aos | 5 | 79 | 1140 | 18294 | 73735 |
| expert_soa | 3 | 55 | 880 | 12634 | 50157 |
| expert | 3 | 50 | 761 | 11293 | 45110 |

Phases of each C form's best run, in microseconds:

| form | 512 | 8192 | 131072 | 2097152 | 8388608 |
|---|---|---|---|---|---|
| naive | make 20 filter 1 measure 3 | make 273 filter 23 measure 50 | make 4693 filter 580 measure 847 | make 70322 filter 24919 measure 32184 | make 294199 filter 98254 measure 125137 |
| expert_aos | make 1 filter 1 measure 1 | make 38 filter 18 measure 23 | make 478 filter 285 measure 376 | make 7544 filter 4688 measure 6060 | make 29929 filter 18887 measure 24918 |
| expert_soa | make 2 filter 0 measure 1 | make 37 filter 2 measure 15 | make 601 filter 35 measure 243 | make 7976 filter 721 measure 3936 | make 30400 filter 4365 measure 15391 |
| expert | make 2 filter 0 measure 1 | make 37 filter 1 measure 11 | make 537 filter 29 measure 194 | make 7763 filter 602 measure 2927 | make 30821 filter 2602 measure 11686 |

## spreadsheet_recalculation

| form | 512 | 8192 | 131072 | 2097152 | 8388608 |
|---|---|---|---|---|---|
| spite | 69 | 1495 | 24931 | 1818127 | 11060778 |
| naive | 55 | 1087 | 26170 | 1588812 | 10204602 |
| expert_aos | 34 | 504 | 16072 | 799323 | 3646334 |
| expert_soa | 29 | 469 | 14056 | 300166 | 2121600 |
| expert | 26 | 449 | 13726 | 239187 | 1696455 |

Phases of each C form's best run, in microseconds:

| form | 512 | 8192 | 131072 | 2097152 | 8388608 |
|---|---|---|---|---|---|
| naive | make 29 recalculate 25 place 0 | make 448 recalculate 629 place 9 | make 6116 recalculate 19888 place 165 | make 104297 recalculate 1466693 place 17821 | make 424886 recalculate 9675195 place 104520 |
| expert_aos | make 12 recalculate 21 place 0 | make 111 recalculate 388 place 5 | make 1280 recalculate 14699 place 91 | make 20450 recalculate 773882 place 4990 | make 81244 recalculate 3543111 place 21977 |
| expert_soa | make 9 recalculate 19 place 0 | make 130 recalculate 333 place 5 | make 1502 recalculate 12468 place 85 | make 17185 recalculate 281544 place 1436 | make 67088 recalculate 2048303 place 6209 |
| expert | make 6 recalculate 20 place 0 | make 113 recalculate 330 place 5 | make 1136 recalculate 12471 place 119 | make 16383 recalculate 221030 place 1773 | make 67250 recalculate 1622099 place 7105 |

## records_sorted_by_one_field

| form | 512 | 8192 | 131072 | 2097152 | 8388608 |
|---|---|---|---|---|---|
| spite | 59 | 961 | 20335 | 615056 | 3064432 |
| naive | 64 | 1208 | 23253 | 724360 | 3728504 |
| expert_aos | 35 | 486 | 5990 | 140397 | 594798 |
| expert_soa | 17 | 254 | 3776 | 99772 | 593781 |
| expert | 22 | 280 | 3852 | 95620 | 422828 |

Phases of each C form's best run, in microseconds:

| form | 512 | 8192 | 131072 | 2097152 | 8388608 |
|---|---|---|---|---|---|
| naive | make 23 sort 39 walk 2 | make 343 sort 823 walk 42 | make 5214 sort 16390 walk 1648 | make 83969 sort 555606 walk 84784 | make 346925 sort 2801422 walk 580155 |
| expert_aos | make 9 sort 23 walk 2 | make 82 sort 370 walk 34 | make 1194 sort 4253 walk 543 | make 18930 sort 109950 walk 11516 | make 87762 sort 456233 walk 50802 |
| expert_soa | make 8 sort 7 walk 2 | make 128 sort 82 walk 43 | make 1318 sort 1234 walk 1222 | make 18756 sort 28009 walk 53006 | make 80373 sort 144622 walk 368785 |
| expert | make 10 sort 10 walk 2 | make 90 sort 155 walk 34 | make 1191 sort 2100 walk 561 | make 19968 sort 63650 walk 12001 | make 77929 sort 294833 walk 50066 |

