# midisplit
An opensource-tool for splitting miditracks to get monophonic tracks

Help (-h)

Usage: midisplit filename.mid outname
Splits a midifile containing midi-tracks into multiple midi-tracks,
that are monophonic. Thy can be used to feed monophonic synthesizers,
like hardware analog-synths.
Parameter 1 selects the midi-file to be imported.
Parameter 2 is the name-start of the created output-files.

Example:
midisplit filename.mid ABC
Splits the midifile with name filename.mid.
Output-files start with letters ABC.

-h as first parameter shows this help.

Some hints:

There are midi-files that this program can't handle. I have not realized what the differing codes inside these files mean.
Its possible that you have to export new midi-files from downloded ones.
In MusE, i have choosen an export-way that does not add more info. I have choosen to not use running status in midi-exports.

The program can handle midi-files with multiple input-tracks. Each of these tracks is loaded and splitted into monophonic tracks.
Tracks that contain no notes are not exported.
Midi-Meta-Events are onl exported to the new files at the beginning of the song.

Have Phun!
