" General
set nocompatible

filetype plugin on

" Encoding
let &termencoding=&encoding
set fileencodings=utf-8,gb18030,gbk,gb2312

" Display
set number
syntax on 
set showmatch
colorscheme molokai
set t_Co=256

set tabstop=4
set shiftwidth=4
set softtabstop=4
set cindent

" Movement
imap <C-F> <Right>
imap <C-L> <Right>
imap <C-B> <Left>
imap <C-K> <Up>
imap <C-J> <Down>
imap <C-o> <ESC>o
