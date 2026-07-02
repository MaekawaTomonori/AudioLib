#ifndef AUDIO_RESULT_HPP_
#define AUDIO_RESULT_HPP_

#include <string>

namespace Audio {

    /** Outcome of a load operation.
     *  Evaluates to true on success, false on failure.
     *  On failure, what() returns the reason as a string.
     */
    class Result {
        std::string error_;
        bool        success_ = false;

    public:
        Result() = default;

        Result(bool _success, const std::string& _error = {})
            : success_(_success), error_(_error) {}

        /** Returns true if the operation succeeded */
        explicit operator bool() const { return success_; }

        /** Returns the error description. Empty string on success. */
        const std::string& what() const { return error_; }
    };

} // namespace Audio

#endif // AUDIO_RESULT_HPP_
