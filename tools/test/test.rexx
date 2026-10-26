/* the real ARexx talks to amigamand */
options results
address AMIGAMAND
'ITER 64'
'PALETTE fire'
'MODE 0'
'WAIT'
'QUERY'
say 'query:' result
'ZOOM -0.75 0 3.5'
'WAIT'
'SAVE SYS:rx.iff'
say 'save rc' rc
'NOSUCHCOMMAND'
say 'bad rc' rc
'PALETTE nothing'
say 'bad palette rc' rc
'QUIT'
say 'quit rc' rc
say 'end'
