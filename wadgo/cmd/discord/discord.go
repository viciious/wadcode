package main

import (
	"bytes"
	"errors"
	"flag"
	"fmt"
	"io"
	"net/http"
	"os"

	"github.com/bwmarrin/discordgo"
	"github.com/viciious/wadcode/wadgo"
	"golang.org/x/text/cases"
	"golang.org/x/text/language"
)

func downloadDiscordAttachment(url string) ([]byte, error) {
	req, err := http.NewRequest(http.MethodGet, url, nil)
	if err != nil {
		return nil, err
	}

	resp, err := http.DefaultClient.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	if resp.StatusCode >= http.StatusMultipleChoices {
		data, _ := io.ReadAll(resp.Body)
		return nil, fmt.Errorf("unexpected status %d: %s", resp.StatusCode, data)
	}
	return io.ReadAll(resp.Body)
}

func saveTempFile(d []byte) (string, error) {
	f, err := os.CreateTemp("", "")
	if err != nil {
		return "", err
	}
	defer f.Close()
	os.WriteFile(f.Name(), d, 0666)
	return f.Name(), nil
}

func saveDiscordAttachment(url string) (string, []byte, error) {
	data, err := downloadDiscordAttachment(url)
	if err != nil {
		return "", nil, err
	}
	if len(data) < 4 {
		return "", nil, errors.New("bad file size")
	}

	fn, err := saveTempFile(data)
	if err != nil {
		return "", nil, err
	}
	return fn, data[:4], err
}
func messageCreate(s *discordgo.Session, m *discordgo.MessageCreate) {
	if m.Author.ID == s.State.User.ID {
		return
	}

	channel, err := s.Channel(m.ChannelID)
	if err != nil {
		fmt.Println(err)
		return
	}
	if channel.Type != discordgo.ChannelTypeDM {
		return
	}

	ref := &discordgo.MessageReference{m.ID, m.ChannelID, m.GuildID}

	switch m.Content {
	case "!patchrom":
		var pwadIndex int
		var files [2]string
		var orgFns [2]string

		if m.Author != nil {
			fmt.Printf("Command '%s' from %#v\n", m.Content, m.Author)
		}

		if len(m.Attachments) != 2 {
			s.ChannelMessageSendReply(channel.ID, "Needs ROM and PWAD attachments", ref)
			return
		}

		for i := 0; i < 2; i++ {
			fn, header, err := saveDiscordAttachment(m.Attachments[i].URL)
			if err != nil {
				fmt.Println(err)
				s.ChannelMessageSendReply(channel.ID, "Error creating file for attachment", ref)
				return
			}
			if bytes.Equal(header, []byte{50, 57, 41, 44}) { // PWAD
				pwadIndex = i
			}
			files[i] = fn
			orgFns[i] = m.Attachments[i].Filename
			defer os.Remove(fn)
		}
		/*
			old := os.Stdout
			prout, pwout, _ := os.Pipe()
			os.Stdout = pwout
			tout := io.TeeReader(prout, os.Stdout)
			bout := bytes.NewBuffer(nil)
			wg := sync.WaitGroup{}

			func() {
				wg.Add(1)
				go func() {
					defer wg.Done()
					io.Copy(bout, tout)
				}()
			}()
		*/
		rom, err := wadgo.PatchRomFile(files[pwadIndex^1], files[pwadIndex])
		/*
			wg.Wait()
			os.Stdout = old
		*/
		if err != nil {
			fmt.Println(err)
		}

		/*
			s.ChannelFileSendWithMessage(channel.ID, "Output log", "log.txt", bout)
		*/
		if err != nil {
			s.ChannelMessageSendReply(channel.ID, err.Error(), ref)
		} else {
			fn := orgFns[pwadIndex^1]
			fn = cases.Title(language.English, cases.NoLower).String(fn)
			fn = "my" + fn
			s.ChannelFileSendWithMessage(channel.ID, "Here's your patched ROM file", fn, bytes.NewReader(rom))
		}
	case "!wad":
		if m.Author != nil {
			fmt.Printf("Command '%s' from %#v\n", m.Content, m.Author)
		}

		if len(m.Attachments) != 1 {
			s.ChannelMessageSendReply(channel.ID, "!wad needs a ROM attachment", ref)
			return
		}

		fn, _, err := saveDiscordAttachment(m.Attachments[0].URL)
		if err != nil {
			fmt.Println(err)
			s.ChannelMessageSendReply(channel.ID, "Error creating file for attachment", ref)
			return
		}

		defer os.Remove(fn)

		_, wad, err := wadgo.SplitRomFile(fn)
		if err != nil {
			fmt.Println(err)
		}

		if err != nil {
			s.ChannelMessageSendReply(channel.ID, err.Error(), ref)
			return
		} else {
			fn = m.Attachments[0].Filename
			fn = cases.Title(language.English, cases.NoLower).String(fn)
			fn = "my" + fn + ".wad"
			s.ChannelFileSendWithMessage(channel.ID, "Here's your WAD file", fn, bytes.NewReader(wad))
		}
	default:
		s.ChannelMessageSendReply(channel.ID, "No idea what you're talking about", ref)
	}
}

func main() {
	tokFlag := flag.String("token", "", "discord token")
	flag.Parse()

	// Create a new Discord session using the provided bot token.
	dg, err := discordgo.New("Bot " + *tokFlag)
	if err != nil {
		fmt.Println("error creating Discord session,", err)
		return
	}

	// Register the messageCreate func as a callback for MessageCreate events.
	dg.AddHandler(messageCreate)

	// In this example, we only care about receiving message events.
	dg.Identify.Intents = discordgo.MakeIntent(discordgo.IntentsDirectMessages | discordgo.IntentsGuildMessages)

	// Open a websocket connection to Discord and begin listening.
	err = dg.Open()
	if err != nil {
		fmt.Println("error opening connection,", err)
		return
	}
	defer dg.Close()

	select {}
}
