# skred
![skred logo](skred.png)

| symbol | description | example |
| :--- | :--- | :--- |
| v | sets current voice (0-63) | v0 |
| a | amplitude | a4 |
| f | frequency | f440.5 |
| w | waveform | w2 |
| p | pan | p-1 |

| wave# | description |
| :--- | :--- |
| 0 | sine |
| 2 | saw down |
| 3 | saw up |
| 4 | triangle |
| 5 | low-period noise |
| 6 | high-period noise |
| 100 to 166 | basic samples |
| 200 to 999 | user wave slots |

## build
```
make skred
```

## run
```
./skred
```

# sk8r
![sk8r logo](sk8r/icon.png)
## build
```
make sk8r-linux
```
## run
```
./build/sk8r-linux
```

# sk8-pad
![sk8-pad logo](sk8-pad/icon.png)
## build
```
make pad-linux
```
## run
```
./build/sk8-pad-linux
```

# midi-sk8
![midi-sk8 logo](midi-sk8/icon.png)
## build
```
make midi-linux
```
## run
```
./build/midi-sk8-linux
```

## background

- needs asound on linux
  - sudo apt install libasound2-dev
