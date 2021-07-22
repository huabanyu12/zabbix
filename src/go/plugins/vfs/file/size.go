/*
** Zabbix
** Copyright (C) 2001-2021 Zabbix SIA
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
**/

package file

import (
	"bytes"
	"errors"
	"fmt"
	"io"
	"os"
)

// Export -
func (p *Plugin) exportSize(params []string) (result interface{}, err error) {
	if len(params) == 0 || len(params) > 2 {
		return nil, errors.New("Invalid number of parameters.")
	}
	if "" == params[0] {
		return nil, errors.New("Invalid first parameter.")
	}
	mode := "bytes"
	if len(params) == 2 && len(params[1]) != 0 {
		mode = params[1]
	}

	switch mode {
	case "bytes":
		if f, err := stdOs.Stat(params[0]); err == nil {
			return f.Size(), nil
		} else {
			return nil, fmt.Errorf("Cannot obtain file information: %s", err.Error())
		}
	case "lines":
		return lineCounter(params[0])
	default:
		return nil, errors.New("Invalid second parameter.")
	}
}

// lineCounter - count number of line in file
func lineCounter(fileName string) (result interface{}, err error) {
	var file *os.File
	if file, err = os.Open(fileName); err != nil {
		return nil, fmt.Errorf("Invalid first parameter: %s", err.Error())
	}
	defer file.Close()
	buf := make([]byte, 64*1024)
	var count int64 = 0
	lineSep := []byte{'\n'}

	for {
		c, err := file.Read(buf)
		count += int64(bytes.Count(buf[:c], lineSep))

		switch {
		case err == io.EOF:
			return count, nil
		case err != nil:
			return nil, fmt.Errorf("Invalid file content: %s", err.Error())
		}
	}
}
