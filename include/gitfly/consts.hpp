#pragma once
#include <cstdint>
#include <string_view>

namespace gitfly::consts {

// Directory and file names
inline constexpr std::string_view kGitDir      = ".gitfly";
inline constexpr std::string_view kObjectsDir  = "objects";
inline constexpr std::string_view kRefsDir     = "refs";
inline constexpr std::string_view kHeadsDir    = "heads";
inline constexpr std::string_view kTagsDir     = "tags";
inline constexpr std::string_view kHeadFile    = "HEAD";
inline constexpr std::string_view kMergeHead   = "MERGE_HEAD";
inline constexpr std::string_view kDefaultBranch = "master";


// Git object type strings
inline constexpr std::string_view kTypeBlob    = "blob";
inline constexpr std::string_view kTypeTree    = "tree";
inline constexpr std::string_view kTypeCommit  = "commit";

// File modes (octal)
inline constexpr std::uint32_t kModeFile = 0100644; // regular file
inline constexpr std::uint32_t kModeTree = 0040000; // directory entry in tree

// ——— Object ID sizes ———
inline constexpr std::size_t kOidRawLen = 20;  // 20 bytes (SHA-1)
inline constexpr std::size_t kOidHexLen = 40;  // 40 hex chars (SHA-1)

// ——— Object store fanout ———
inline constexpr std::size_t kFanoutDirHexLen = 2; // "aa/" + "bbbb..." in .gitfly/objects

// ——— Port number ———
inline constexpr int portNumber = 9418;

// ——— Commit header prefixes (used in parsing/formatting) ———
inline constexpr std::string_view kTreePrefix      = "tree ";
inline constexpr std::string_view kRefPrefix      = "ref: ";
inline constexpr std::string_view kParentPrefix    = "parent ";
inline constexpr std::string_view kAuthorPrefix    = "author ";
inline constexpr std::string_view kCommitterPrefix = "committer ";

// ——— Common characters ———
inline constexpr char kSpace = ' ';
inline constexpr char kNul   = '\0';
inline constexpr char kLF    = '\n';

// ——— Protocol (if you want constants for your TCP demo) ———
inline constexpr std::string_view kHelloLine   = "HELLO 1";
inline constexpr std::string_view kOpPush      = "OP PUSH ";
inline constexpr std::string_view kOpClone     = "OP CLONE";
inline constexpr std::string_view kOpFetch     = "OP FETCH";
inline constexpr std::string_view kTokNobj     = "NOBJ ";
inline constexpr std::string_view kTokObj      = "OBJ ";
inline constexpr std::string_view kTokDone     = "DONE";
inline constexpr std::string_view kTokOkGo     = "OKGO";
inline constexpr std::string_view kTokOk       = "OK";
} // namespace gitfly::consts

 