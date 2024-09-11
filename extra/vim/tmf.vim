" Quit when a syntax file was already loaded.
if exists('b:current_syntax') | finish|  endif

syntax region tmfExtOps start="(" end=")" containedin=tmfStateDest
syntax sync match tmfExtOpsStart groupthere tmfExtOps "("
syntax sync match tmfExtOpsEnd groupthere NONE ")"
syntax match tmfStateDef "\(\s\|\n\)*\w\+\(\s\|\n\)*:\(\s\|\n\)*" containedin=TOP,tmfComment,tmfCommentML
syntax match tmfGreaterThan ">\('\)\@!" containedin=TOP,tmfComment,tmfCommentML,tmfExtOps
syntax match tmfEquals "\w\+\(\s\|\n\)*=" containedin=TOP,tmfComment,tmfCommentML,tmfExtOps
syntax match tmfSetDifference "\\\('\)\@!" containedin=TOP,tmfComment,tmfCommentML,tmfExtOps
syntax match tmfSemicolonEnd "\(;\('\)\@!\)\|\('\_.'\zs;\ze'\_.'\)" containedin=TOP,tmfComment,tmfCommentML,tmfExtOps

syntax match tmfComment "#\('\)\@!.*\_." containedin=ALLBUT,tmfCommentML
syntax region tmfCommentML start=/#[*]/ end=/[*]#/ containedin=ALL
syntax sync match tmfCommentStart groupthere tmfCommentML "#[*]"
syntax sync match tmCommentEnd groupthere NONE "[*]#"

syntax match tmfVariableRef "[$]\(\w\|\d\)\+\(\s\|\n\|,\|,\|;\|\\\)" containedin=tmfSymList,tmfVarList
syntax match tmfVariableDec "\zs\w\+\ze\(\s\|\n\)*=" containedin=tmfEquals

syntax match tmfListComma "\(\('\_.'\|\_.\)\(\s\|\n\)*\zs,\ze\(\s\|\n\)*'\_.'\)\|\(\(\s\|\n\)*\zs,\ze\(\s\|\n\)*[^']\)" contained containedin=tmfSymList,tmfVarList,tmfVariableRef,tmfStateDest

syntax region tmfStateDest start=">[^']" end=";" containedin=tmfSymList "containedin=TOP,tmfComment,tmfCommentML
syntax sync match tmfStateDestStart groupthere tmfStateDest ">[^']"
syntax sync match tmfStateDestEnd groupthere NONE ";"

syntax region tmfVarList start="\w\+\(\s\|\n\)*=" end=";\('\)\@!" containedin=TOP,tmfComment,tmfCommentML
syntax sync match tmfVarListStart groupthere tmfVarList "\w\+\(\s\|\n\)*="
syntax sync match tmfVarListEnd groupthere NONE ";\('\)\@!"

syntax region tmfSymList start="\w\+\(\s\|\n\)*:\|;" end=/>/ containedin=TOP,tmfComment,tmfCommentML,tmfVarList,tmfSemicolonEnd
syntax sync match tmfSymListStart groupthere tmfSymList "\w\+\(\s\|\n\)*:\|;"
syntax sync match tmfSymListEnd groupthere NONE ">"

syntax region tmfStartDir start="start\(\s\|\n\)*:" end=';' containedin=TOP
syntax sync match tmfStartDirStart groupthere tmfStartDir "start\(\s\|\n\)*:"
syntax sync match tmfStartDirEnd groupthere NONE ";"

hi def link tmfComment Comment
hi def link tmfCommentML Comment
hi def link tmfStateDef ModeMsg
hi def link tmfSymList Title
hi def link tmfListComma Identifier
hi tmfStateDest ctermfg=LightGreen cterm=bold
hi tmfStartDir ctermfg=LightGreen cterm=bold
hi def link tmfVariableDec Identifier
hi def link tmfVarList String
hi def link tmfVariableRef PreProc
hi def link tmfExtOps String
hi def link tmfSymQuoted String
hi def link tmfSemicolonEnd Identifier
hi tmfSetDifference ctermfg=Yellow cterm=bold
hi def link tmfGreaterThan Operator
hi tmfGreaterThan ctermfg=Yellow cterm=bold
hi tmfEquals ctermfg=Yellow cterm=bold

let b:current_syntax = 'tmf'
