baud=230400

picocom --baud $baud /dev/tty_hs_dbg
# kill with ctrl-a ctrl-x

#screen /dev/tty_hs_dbg $baud
# kill with ctrl-a-k

#minicom -b $baud -D /dev/tty_hs_dbg

