#ifndef _PARSE_H_
#define _PARSE_H_
enum {
  FUNC_NULL,
  FUNC_ERR,
  FUNC_SYS,
  FUNC_IMM,
  //
  FUNC_VOICE,
  FUNC_FREQ,
  FUNC_AMP,
  FUNC_TRIGGER,
  FUNC_VELOCITY,
  FUNC_MUTE,
  FUNC_AMOD,
  FUNC_FMOD,
  FUNC_PMOD,
  FUNC_MIDI,
  FUNC_WAVE,
  FUNC_LOOP,
  FUNC_DIR,
  FUNC_INTER,
  FUNC_PAN,
  FUNC_ADSR,
  FUNC_DECI,
  FUNC_QUANT,
  FUNC_RESET,
  // subfunctions
  FUNC_HELP,
  FUNC_SEQSTART,
  FUNC_SEQSTOP,
  FUNC_SEQPAUSE,
  FUNC_SEQRESUME,
  FUNC_QUIT,
  FUNC_STATS0,
  FUNC_STATS1,
  FUNC_TRACE,
  FUNC_DEBUG,
  FUNC_OSCOPE,
  FUNC_LOAD,
  FUNC_SAVE,
  FUNC_WAVEREAD,
  //
  FUNC_SHOWWAVE,
  FUNC_DELAY,
  FUNC_COMMENT,
  FUNC_WHITESPACE,
  FUNC_METRO,
  //
  FUNC_UNKNOWN,
};

char *_func_func_str[FUNC_UNKNOWN+1] = {
  [FUNC_NULL] = "-?-",
  [FUNC_ERR] = "err",
  [FUNC_SYS] = "sys",
  [FUNC_IMM] = "imm",
  [FUNC_VOICE] = "voice",
  [FUNC_FREQ] = "freq",
  [FUNC_AMP] = "amp",
  [FUNC_TRIGGER] = "trigger",
  [FUNC_VELOCITY] = "velocity",
  [FUNC_MUTE] = "mute",
  [FUNC_AMOD] = "amod",
  [FUNC_FMOD] = "fmod",
  [FUNC_PMOD] = "pmod",
  [FUNC_MIDI] = "midi",
  [FUNC_WAVE] = "wave",
  [FUNC_LOOP] = "loop",
  [FUNC_DIR] = "dir",
  [FUNC_INTER] = "inter",
  [FUNC_PAN] = "pan",
  [FUNC_ADSR] = "adsr",
  [FUNC_DECI] = "deci",
  [FUNC_QUANT] = "quant",
  [FUNC_RESET] = "reset",
  [FUNC_UNKNOWN] = "unknown",
  //
  [FUNC_HELP] = "help",
  [FUNC_SEQSTART] = "seq-start",
  [FUNC_SEQSTOP] = "seq-stop",
  [FUNC_SEQPAUSE] = "seq-pause",
  [FUNC_SEQRESUME] = "seq-resume",
  [FUNC_QUIT] = "quit",
  [FUNC_STATS0] = "stats-0",
  [FUNC_STATS1] = "stats-1",
  [FUNC_TRACE] = "trace",
  [FUNC_DEBUG] = "debug",
  [FUNC_OSCOPE] = "oscope",
  [FUNC_LOAD] = "load",
  [FUNC_SAVE] = "save",
  [FUNC_SHOWWAVE] = "show-wave",
  [FUNC_DELAY] = "delay",
  [FUNC_COMMENT] = "comment",
  [FUNC_WHITESPACE] = "white-space",
  [FUNC_METRO] = "metro",
  [FUNC_WAVEREAD] = "wave-read",
};

char *func_func_str(int n) {
  if (n >= 0 && n <= FUNC_UNKNOWN) {
    if (_func_func_str[n]) {
      return _func_func_str[n];
    }
  }
  return "no-string";
}
#endif
