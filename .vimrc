function! Make(args)
	exec "silent make" . a:args
	"if empty(filter(getqflist(), 'v:val.valid'))
	if !empty(filter(getqflist(), 'match(v:val.text, "Success") >= 0'))
		echo "Compilation succeeded!"
	else
		echo "Compilation failed!"
	endif
endfunction

command! -nargs=* Build  :call Make("<args>")
command! -nargs=* BuildR :call Make("-r <args>")
command! -nargs=* BuildP :call Make("-p <args>")
command! -nargs=* -complete=file Run :silent exec "!start bin\\Win32Platform.exe"
command! -nargs=* Debug :silent exec "!start remedybg physics.rdbg"

compiler msvc
set makeprg=build.bat

let msvc_path = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.34.31933\'
let winsdk_path = 'C:\Program Files (x86)\Windows Kits\10\'
let winsdk_version = '10.0.20348.0'

let $Path .= ';' . msvc_path . 'bin\HostX64\x64'
let $INCLUDE = msvc_path . 'include;' .
			 \ winsdk_path . 'include\' . winsdk_version . '\ucrt;' .
			 \ winsdk_path . 'include\' . winsdk_version . '\shared;' .
			 \ winsdk_path . 'include\' . winsdk_version . '\um;' .
			 \ winsdk_path . 'include\' . winsdk_version . '\winrt;' .
			 \ winsdk_path . 'include\' . winsdk_version . '\cppwinrt'
let $LIB = msvc_path . 'lib\x64;' .
		 \ winsdk_path . 'lib\' . winsdk_version . '\ucrt\x64;' .
		 \ winsdk_path . 'lib\' . winsdk_version . '\um\x64'

command! Assemble :call term_sendkeys("terminal", "assembleOutput.bat\<CR>") | call ShowTerminal()

" Ignore build folder in wildcards
set wildignore+=*/build/*
set wildignore+=*/bin/*

call StartTerminal()
call HideTerminal()
