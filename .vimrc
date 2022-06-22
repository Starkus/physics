let g:build_command = "build.bat"
let g:build_release_command = "build.bat -r"
let g:run_command =  "bin\\Win32Platform.exe"
let g:debug_command =  "remedybg.exe gjk.rdbg"

" Ignore build folder in wildcards
set wildignore+=*/bin/*

call StartTerminal()
call HideTerminal()
