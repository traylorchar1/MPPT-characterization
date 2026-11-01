#!/bin/sh
# sed -i -- 's/foo/bar/g' *

# copyrights

COPY_2008="Copyright (c) 2008, Samuel Lenius"
COPY_2009="Copyright (c) 2009, Samuel Lenius"
COPY_2010="Copyright (c) 2010, Samuel Lenius"
COPY_2011="Copyright (c) 2011, Samuel Lenius"
COPY_2012="Copyright (c) 2012, Samuel Lenius"
COPY_2013="Copyright (c) 2013, Samuel Lenius"
COPY_2014="Copyright (c) 2014, Samuel Lenius"
COPY_2015="Copyright (c) 2015, Samuel Lenius"

# 65 char start
COM_65_START="\/\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*"

# 65 char end
COM_65_END="\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\/"

# 65 char comment
COM_65="\/\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\/"

# multi pattern match char comment
COM_BIG="\/\*\{50,120\}\/"
COM_BIG_START="\/\*\{80,120\}"
COM_BIG_END="\*\{80,120\}\/"

# 80 char comment
COM_80="\/\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\/"
COM_80_START="\/\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*"
COM_80_END="\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\/"


echo $COPY_2015

# fix all the copyrights
sed -i -- "s/$COPY_2008/$COPY_2015/g" *.c
sed -i -- "s/$COPY_2008/$COPY_2015/g" *.h

sed -i -- "s/$COPY_2009/$COPY_2015/g" *.c
sed -i -- "s/$COPY_2009/$COPY_2015/g" *.h

sed -i -- "s/$COPY_2010/$COPY_2015/g" *.c
sed -i -- "s/$COPY_2010/$COPY_2015/g" *.h

sed -i -- "s/$COPY_2011/$COPY_2015/g" *.c
sed -i -- "s/$COPY_2011/$COPY_2015/g" *.h

sed -i -- "s/$COPY_2012/$COPY_2015/g" *.c
sed -i -- "s/$COPY_2012/$COPY_2015/g" *.h

sed -i -- "s/$COPY_2013/$COPY_2015/g" *.c
sed -i -- "s/$COPY_2013/$COPY_2015/g" *.h

sed -i -- "s/$COPY_2014/$COPY_2015/g" *.c
sed -i -- "s/$COPY_2014/$COPY_2015/g" *.h

# replace all 65 char comments with 80 char
sed -i -- "s/$COM_80/$COM_65/g" *.c
sed -i -- "s/$COM_80/$COM_65/g" *.h

sed -i -- "s/$COM_80_START/$COM_65_START/g" *.c
sed -i -- "s/$COM_80_START/$COM_65_START/g" *.h

sed -i -- "s/$COM_80_END/$COM_65_END/g" *.c
sed -i -- "s/$COM_80_END/$COM_65_END/g" *.h

sed -i -- "s/$COM_65_START/$COM_80_START/g" *.c
sed -i -- "s/$COM_65_START/$COM_80_START/g" *.h

sed -i -- "s/$COM_65_END/$COM_80_END/g" *.c
sed -i -- "s/$COM_65_END/$COM_80_END/g" *.h

sed -i -- "s/$COM_65/$COM_80/g" *.c
sed -i -- "s/$COM_65/$COM_80/g" *.h

# replace all multi char commets with 80 char

echo "s/$COM_BIG/$COM_80/g"
sed -iv -- "s/$COM_BIG/$COM_80/g" *.c
sed -iv -- "s/$COM_BIG/$COM_80/g" *.h

sed -iv -- "s/$COM_BIG_END/$COM_80_END/g" *.c
sed -iv -- "s/$COM_BIG_END/$COM_80_END/g" *.h

sed -iv -- "s/$COM_BIG_START/$COM_80_START/g" *.c
sed -iv -- "s/$COM_BIG_START/$COM_80_START/g" *.h

