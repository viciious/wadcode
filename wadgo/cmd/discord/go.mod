module github.com/viciious/wadcode/wadgo/cmd/discord

go 1.18

replace github.com/viciious/wadcode/wadgo => ../../

require (
	github.com/bwmarrin/discordgo v0.26.1
	github.com/viciious/wadcode/wadgo v0.0.0-00010101000000-000000000000
	golang.org/x/text v0.3.3
)

require (
	github.com/gorilla/websocket v1.4.2 // indirect
	golang.org/x/crypto v0.0.0-20210421170649-83a5a9bb288b // indirect
	golang.org/x/sys v0.0.0-20201119102817-f84b799fce68 // indirect
)
