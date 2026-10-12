# Totals over every report the high level comparison wrote, every function it does
# not cover and why, and every difference, which must be none.
#
# A report line is "not covered <how many> <function> <kind>: <why>", and a
# function's name has no spaces in it, so the count is the third word, the
# function the fourth, and the kind and the reason everything after that.
  /^high level walked/ { programs += 1; walked += $4; agreed += $6; differed += $8 }
  /^kinds compared/ { compared += $3; skipped += $7 }
  /^calls not compared/ { calls += $4 }
  /^difference/ { differences += 1; print "difference " substr($0, 11) }
  /^not covered [0-9]/ {
      count += $3
      kinds[$5] += 1
      why = $5
      for (part = 6; part <= NF; part = part + 1) { why = why " " $part }
      reasons[why] += 1
  }
  END {
      print "programs with a report " programs
      print "functions walked " walked
      print "functions agreed " agreed
      print "functions differed " differed
      print "kinds of fact compared " compared
      print "kinds of fact not covered " skipped
      print "functions whose calls are not compared " calls
      print "differences " differences
      print "functions not covered " count
      for (reason in reasons) print "not covered " reasons[reason], reason
  }
