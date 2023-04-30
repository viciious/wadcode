package main

import (
	"fmt"
	"os"

	"github.com/viciious/wadcode/wadgo"
)

func main() {
	if len(os.Args) < 4 {
		fmt.Printf("Usage: %s <INROM> <PWAD> <OUTROM>\n", os.Args[0])
		os.Exit(1)
	}
	err := wadgo.PatchRomToFile(os.Args[1], os.Args[2], os.Args[3])
	if err != nil {
		fmt.Println(err)
	}
}
