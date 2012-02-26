#! /bin/csh
#   ask
glob "Overwirite?"
set ans = ""
while (($ans != "y") && (and != "n"))
    glob " (y or n) "; set ans = $<
end

if ($ans == "y") echo YES ; echo YES; exit
echo NO
echo NO
