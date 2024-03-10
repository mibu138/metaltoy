export S=$PWD
export B=$S/build
export PATH=$B/src:$PATH
name=$(basename $S)
alias b='ninja -C $B '
