#!/bin/bash

# There was a too ex-pen-sive air-port su-shi place in the Frank-furt Air-port
# that had these cute lit-tle sin-gle ser-ving soy sauce dis-pen-sers

# i went to the frank-furt air-port
# 1 2    3  4   5     6    7   8
# got hun-gry what what will i eat
# 1   2   3   4    5    6    7 8
# that ex-pen-sive air-port su-shi
# 1    2  3   4    5   6    7  8
# had fish-sha-ped soy dis-pen-sers
# 1   2    3   4   5   6   7   8
# cute lit-tle soy sauce dis-pen-sers
# 1    2   3   4   5     6   7   8
# sha-ped like fish
# 1   2   3    4
# put it in your ear
# 1   2  3  4    5
# i i i have a cute tee-ny ti-ny soy sauce fish in my ear
# 1 2 3 4    5 6    7   8  9  10 11  12    13   14 15 16
# fish in my ear
# 1    2  3  4
# fish in my rear
# 1    2  3  4
# for-got i was hun-gry dam it
# 1   2   3 4   5   6   7   8
all="There was a too expensive airport sushi place in the Frankfurt Airport that had these cute little single serving soy sauce dispensers Put it in your ear I've got a teeny tiny soy sauce fish in my ear Fish in my ear Fish in my rear"
dict=$(echo $all | sed 's/ /\n/g' | tr '[:upper:]' '[:lower:]' | sort | uniq)
count=200
for word in $dict
do
  printf "%-10s wave%s.wav\n" $word $count
  file=$(printf "wave%s.wav" $count)
  # espeak $word
  espeak $word -w bozo.wav && sox bozo.wav -r 44100 $file
  let count++
done

espeak "put it in your ear" -w bozo.wav && sox bozo.wav -r 44100 wave300.wav
espeak "fish in my ear" -w bozo.wav && sox bozo.wav -r 44100 wave301.wav


exit

espeak "su she" -w bozo.wav ; sox bozo.wav -r 44100 wave100.wav
