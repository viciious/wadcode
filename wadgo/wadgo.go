package wadgo

/*
#include <stdlib.h>
int wad32x_main(int argc, char *argv[]);
*/
import "C"
import (
	"bytes"
	"encoding/binary"
	"encoding/json"
	"errors"
	"fmt"
	"os"
	"os/exec"
	"path"
	"path/filepath"
	"strings"
	"unsafe"
)

const wadOffset = 0x30000
const wadCodeDir = "../../../"

type wadContents []map[string]interface{}

const romPadding = 512 * 1024

func runWad32xImport(inFile, outFile string) {
	args := []string{"wad32x", "-import", inFile, outFile}

	cArgs := C.malloc(C.size_t(len(args)) * C.size_t(unsafe.Sizeof(uintptr(0))))
	defer C.free(unsafe.Pointer(cArgs))

	for i := 0; i < len(args); i++ {
		ca := (*[1<<30 - 1]*C.char)(cArgs)
		ca[i] = C.CString(args[i])
		defer C.free(unsafe.Pointer(ca[i]))
	}

	C.wad32x_main(C.int(len(args)), (**C.char)(cArgs))
}

func SplitRom(data []byte) ([]byte, []byte, error) {
	wadOffset := 0
	for i := 0x1000; i < len(data)-5; i += 0x1000 {
		if bytes.Equal(data[i:i+4], []byte{'I', 'W', 'A', 'D'}) {
			wadOffset = i
			break
		}
	}

	if wadOffset == 0 {
		return nil, nil, errors.New("IWAD not found in ROM")
	}
	return data[:wadOffset], data[wadOffset:], nil
}

func SplitRomFile(inFile string) ([]byte, []byte, error) {
	data, err := os.ReadFile(inFile)
	if err != nil {
		return nil, nil, err
	}
	return SplitRom(data)
}

func decompileWad(inWad, outDir string, bo binary.ByteOrder) error {
	var cmd *exec.Cmd

	fmt.Printf("Decompiling %s\n", inWad)

	if bo == binary.BigEndian {
		cmd = exec.Command("./wadcode", "decompile", "--big-endian", inWad, outDir)
	} else {
		cmd = exec.Command("./wadcode", "decompile", inWad, outDir)
	}

	cmd.Dir = wadCodeDir
	stdoutStderr, err := cmd.CombinedOutput()
	fmt.Println(strings.Join(cmd.Args, " "))
	fmt.Println(string(stdoutStderr))
	if err != nil {
		return err
	}
	return nil
}

func compileWad(inDir, outWad string, ssf bool, wadOffset int) error {
	fmt.Printf("Compiling %s to %s\n", inDir, outWad)

	wadCodeArgs := []string{"compile", "--big-endian"}
	if ssf {
		wadCodeArgs = append(wadCodeArgs, "--ssf", "--baseoffset", fmt.Sprintf("%d", wadOffset))
	}
	wadCodeArgs = append(wadCodeArgs, inDir, outWad)

	cmd := exec.Command("./wadcode", wadCodeArgs...)
	cmd.Dir = wadCodeDir
	stdoutStderr, err := cmd.CombinedOutput()
	fmt.Println(strings.Join(cmd.Args, " "))
	fmt.Println(string(stdoutStderr))
	if err != nil {
		return err
	}
	return nil
}

func getWadContents(dir string) wadContents {
	var result wadContents

	b, _ := os.ReadFile(path.Join(dir, "content.json"))
	json.Unmarshal(b, &result)
	//fmt.Println(result)
	return result
}

func (lumps wadContents) findFirstLumpByName(lump string) int {
	for i, l := range lumps {
		if n, ok := l["name"]; ok && n == lump {
			return i
		}
	}
	return -1
}

func (lumps wadContents) findLastLumpByName(lump string) int {
	for i := len(lumps) - 1; i >= 0; i-- {
		l := lumps[i]
		if n, ok := l["name"]; ok && n == lump {
			return i
		}
	}
	return -1
}

func replaceLumpFiles(in wadContents, inDir string, firstIn, lastIn int,
	out wadContents, outDir string, firstOut, lastOut int, filter func(string) bool) wadContents {

	//fmt.Println(firstIn, lastIn, firstOut, lastOut)
	if firstIn < 0 || lastIn < 0 || firstIn > lastIn || firstIn >= len(in) {
		//return errors.New("invalid lump order in custom WAD")
		return out
	}
	if firstOut < 0 || lastOut < 0 || firstOut > lastOut || firstOut >= len(out) {
		//return errors.New("invalid lump order in origin WAD")
		return out
	}

	var skipped wadContents

	for i := firstOut; i <= lastOut; i++ {
		if filter != nil {
			if n, ok := out[i]["name"]; !ok || !filter(n.(string)) {
				skipped = append(skipped, out[i])
				continue
			}
		}

		if fn, ok := out[i]["filename"]; ok {
			ffn := path.Join(outDir, "files", fn.(string))
			fmt.Printf("Removing %s\n", ffn)
			os.Remove(ffn)
			continue
		}
	}

	var added wadContents

	for i := firstIn; i <= lastIn; i++ {
		if filter != nil {
			if n, ok := in[i]["name"]; ok && !filter(n.(string)) {
				continue
			}
		}

		if fn, ok := in[i]["filename"]; ok {
			sfn := path.Join(inDir, "files", fn.(string))
			dfn := path.Join(outDir, "files", fn.(string))
			fmt.Printf("Linking %s to %s\n", sfn, dfn)
			os.Link(sfn, dfn)
		}
		added = append(added, in[i])
	}

	res := make(wadContents, 0)
	res = append(res, out[0:firstOut]...)
	res = append(res, added...)
	res = append(res, skipped...)
	res = append(res, out[lastOut+1:]...)
	return res
}

func findFirstLumpsByName(lump string, c1 wadContents, c2 wadContents) (int, int) {
	return c1.findFirstLumpByName(lump), c2.findFirstLumpByName(lump)
}

func findLastLumpsByName(lump string, c1 wadContents, c2 wadContents) (int, int) {
	return c1.findLastLumpByName(lump), c2.findLastLumpByName(lump)
}

func replaceLump(lump string, in wadContents, inDir string, out wadContents, outDir string) wadContents {
	lin, lout := findFirstLumpsByName(lump, in, out)
	return replaceLumpFiles(in, inDir, lin, lin, out, outDir, lout, lout, nil)
}

func replaceMaps(in wadContents, inDir string, out wadContents, outDir string) wadContents {
	mapLumps := map[string]bool{
		"things":   true,
		"linedefs": true,
		"sidedefs": true,
		"vertexes": true,
		"segs":     true,
		"ssectors": true,
		"nodes":    true,
		"sectors":  true,
		"reject":   true,
		"blockmap": true,
	}
	filter := func(n string) bool {
		if strings.Contains(strings.ToLower(n), "map") {
			return true
		}
		if _, ok := mapLumps[strings.ToLower(n)]; ok {
			return true
		}
		return false
	}
	firstIn, firstOut := findFirstLumpsByName("THINGS", in, out)
	lastIn, lastOut := findLastLumpsByName("BLOCKMAP", in, out)
	out = replaceLumpFiles(in, inDir, firstIn-1, lastIn, out, outDir, firstOut-1, lastOut, filter)
	return replaceLump("DMAPINFO", in, inDir, out, outDir)
}

func replaceTextures(in wadContents, inDir string, out wadContents, outDir string) wadContents {
	firstIn, firstOut := findFirstLumpsByName("T_START", in, out)
	lastIn, lastOut := findFirstLumpsByName("T_END", in, out)
	out = replaceLumpFiles(in, inDir, firstIn, lastIn, out, outDir, firstOut, lastOut, nil)
	return replaceLump("TEXTURE1", in, inDir, out, outDir)
}

func replaceFlats(in wadContents, inDir string, out wadContents, outDir string) wadContents {
	firstIn, firstOut := findFirstLumpsByName("F_START", in, out)
	lastIn, lastOut := findFirstLumpsByName("F_END", in, out)
	return replaceLumpFiles(in, inDir, firstIn, lastIn, out, outDir, firstOut, lastOut, nil)
}

func mergeWadDirs(inDir, outDir string) error {
	cc := getWadContents(inDir)
	sc := getWadContents(outDir)

	res := sc

	res = replaceMaps(cc, inDir, res, outDir)
	res = replaceTextures(cc, inDir, res, outDir)
	res = replaceFlats(cc, inDir, res, outDir)

	prsjon, _ := prettyJSON(res)
	//fmt.Println(string(prsjon))
	//fmt.Println(path.Join(outDir, "content.json"))
	return os.WriteFile(path.Join(outDir, "content.json"), prsjon, 0666)
}

func generateMips(inDir string) error {
	fmt.Printf("Generating mips for %s\n", inDir)

	wadCodeArgs := []string{inDir}

	cmd := exec.Command("./genmips", wadCodeArgs...)
	cmd.Dir = wadCodeDir
	stdoutStderr, err := cmd.CombinedOutput()
	fmt.Println(strings.Join(cmd.Args, " "))
	fmt.Println(string(stdoutStderr))
	if err != nil {
		return err
	}
	return nil
}

func mergeWads(workDir, iwad, pwad, outFn string, ssf bool, wadOffset int) error {
	var err error

	tmpdir, err := os.MkdirTemp(workDir, "")
	if err != nil {
		return err
	}
	//defer os.RemoveAll(tmpdir)

	iwadDir := path.Join(tmpdir, "iwad")
	pwadDir := path.Join(tmpdir, "pwad")

	if err = decompileWad(pwad, pwadDir, binary.BigEndian); err != nil {
		return err
	}

	if err = decompileWad(iwad, iwadDir, binary.BigEndian); err != nil {
		return err
	}

	if err = mergeWadDirs(pwadDir, iwadDir); err != nil {
		return err
	}

	//if err = generateMips(iwadDir); err != nil {
	//	return err
	//}

	if err = compileWad(iwadDir, outFn, ssf, wadOffset); err != nil {
		return err
	}

	return nil
}

func prettyJSON(res interface{}) ([]byte, error) {
	var buffer bytes.Buffer
	enc := json.NewEncoder(&buffer)
	enc.SetIndent("", "    ")

	err := enc.Encode(res)
	if err != nil {
		return nil, err
	}

	return buffer.Bytes(), nil
}

func PatchRom(rom []byte, pwad string) ([]byte, error) {
	tmpdir, err := os.MkdirTemp("", "")
	if err != nil {
		return nil, err
	}
	//defer os.RemoveAll(tmpdir)

	romCode, iwadData, err := SplitRom(rom)
	if err != nil {
		return nil, err
	}

	tmpdir, _ = filepath.Abs(tmpdir)
	iwadTmpFn := path.Join(tmpdir, "iwad.wad")
	pwadTmpFn := path.Join(tmpdir, "pwad.wad")
	resTmpFn := path.Join(tmpdir, "iwad.wad")

	if err = os.WriteFile(iwadTmpFn, iwadData, 0666); err != nil {
		return nil, err
	}

	runWad32xImport(pwad, pwadTmpFn)

	ssf := bytes.Equal(romCode[0x100:0x108], []byte{'S', 'E', 'G', 'A', ' ', 'S', 'S', 'F'})
	fmt.Println(string(romCode[0x100:0x108]), ssf, len(romCode))
	if err = mergeWads(tmpdir, iwadTmpFn, pwadTmpFn, resTmpFn, ssf, len(romCode)); err != nil {
		return nil, err
	}

	wadBytes, err := os.ReadFile(resTmpFn)
	if err != nil {
		return nil, err
	}

	resRom := make([]byte, 0)
	resRom = append(resRom, romCode...)
	resRom = append(resRom, wadBytes...)
	pad := len(resRom) % romPadding
	if pad != 0 {
		pad = romPadding - pad
		resRom = append(resRom, bytes.Repeat([]byte{0}, pad)...)
	}
	return resRom, nil
}

func PatchRomFile(inRomFn, pwad string) ([]byte, error) {
	rom, err := os.ReadFile(inRomFn)
	if err != nil {
		return nil, err
	}
	return PatchRom(rom, pwad)
}

func PatchRomToFile(inRom, pwad, outRom string) error {
	res, err := PatchRomFile(inRom, pwad)
	if err != nil {
		return err
	}

	err = os.WriteFile(outRom, res, 0666)
	if err != nil {
		return err
	}

	return nil
}
