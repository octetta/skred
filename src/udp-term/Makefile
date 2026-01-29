all:
	fyne bundle -package main icon.png > bundled.go
	go build -ldflags="-s -w" -trimpath -o udp-term .
