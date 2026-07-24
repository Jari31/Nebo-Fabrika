
namespace TerminalTextStyling
{
constexpr auto RESET = "\033[0m";

namespace Style
{
constexpr auto BOLD        = "\033[1m";
constexpr auto DIM         = "\033[2m";
constexpr auto ITALIC      = "\033[3m";
constexpr auto UNDERLINE   = "\033[4m";
constexpr auto SLOW_BLINK  = "\033[5m";
constexpr auto RAPID_BLINK = "\033[6m";
constexpr auto REVERSE     = "\033[7m";
constexpr auto CONCEAL     = "\033[8m";
constexpr auto CROSSED_OUT = "\033[9m";

// Style Resets
constexpr auto BOLD_OFF        = "\033[21m";
constexpr auto DIM_OFF         = "\033[22m";
constexpr auto ITALIC_OFF      = "\033[23m";
constexpr auto UNDERLINE_OFF   = "\033[24m";
constexpr auto BLINK_OFF       = "\033[25m";
constexpr auto REVERSE_OFF     = "\033[27m";
constexpr auto CONCEAL_OFF     = "\033[28m";
constexpr auto CROSSED_OUT_OFF = "\033[29m";
} // namespace Style

namespace Foreground
{
constexpr auto BLACK   = "\033[30m";
constexpr auto RED     = "\033[31m";
constexpr auto GREEN   = "\033[32m";
constexpr auto YELLOW  = "\033[33m";
constexpr auto BLUE    = "\033[34m";
constexpr auto MAGENTA = "\033[35m";
constexpr auto CYAN    = "\033[36m";
constexpr auto WHITE   = "\033[37m";

constexpr auto BRIGHT_BLACK   = "\033[90m";
constexpr auto BRIGHT_RED     = "\033[91m";
constexpr auto BRIGHT_GREEN   = "\033[92m";
constexpr auto BRIGHT_YELLOW  = "\033[93m";
constexpr auto BRIGHT_BLUE    = "\033[94m";
constexpr auto BRIGHT_MAGENTA = "\033[95m";
constexpr auto BRIGHT_CYAN    = "\033[96m";
constexpr auto BRIGHT_WHITE   = "\033[97m";
} // namespace Foreground

namespace Background
{
constexpr auto BLACK   = "\033[40m";
constexpr auto RED     = "\033[41m";
constexpr auto GREEN   = "\033[42m";
constexpr auto YELLOW  = "\033[43m";
constexpr auto BLUE    = "\033[44m";
constexpr auto MAGENTA = "\033[45m";
constexpr auto CYAN    = "\033[46m";
constexpr auto WHITE   = "\033[47m";

constexpr auto BRIGHT_BLACK   = "\033[100m";
constexpr auto BRIGHT_RED     = "\033[101m";
constexpr auto BRIGHT_GREEN   = "\033[102m";
constexpr auto BRIGHT_YELLOW  = "\033[103m";
constexpr auto BRIGHT_BLUE    = "\033[104m";
constexpr auto BRIGHT_MAGENTA = "\033[105m";
constexpr auto BRIGHT_CYAN    = "\033[106m";
constexpr auto BRIGHT_WHITE   = "\033[107m";
} // namespace Background
} // namespace TerminalTextStyling
