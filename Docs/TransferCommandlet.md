# ConvaiHTTP file transfer commandlet

The `ConvaiHTTPTransfer` module provides a Windows editor commandlet for large, streamed Cloud Avatars GET and PUT transfers. It uses only Core, CoreUObject, Engine, Json and CONVAIHTTP. The plugin descriptor loads it at Default in Editor targets on Win64; it is not a runtime game module.

This candidate contains main plus the five staging commits through `d40f5b1`, including response-body archive streaming. It moves the already exercised SDK worker into the HTTP plugin, fixes direct-buffer file reads, preserves 64-bit body lengths, accepts case-insensitive Content-Length, and uses curl's large upload-length option for PUT/PATCH. POST no longer replaces its large size with a long-sized option. No SDK-local source patch or game-module template is needed.

## Remote distribution contract

Distribute the complete plugin directory in a versioned release archive. Consumers resolve Convai's published remote dependency configuration, then verify and stage the selected archive using their existing version/hash rules. Do not treat this source change as a published release. Version 1.2.0 is the candidate; `Resources/Transfer/manifest.json` declares its module and stable protocol without tying identity to a commit. The integration owner supplies the final release asset/config and pin.

The archive must contain ConvaiHTTP.uplugin, Source/CONVAIHTTP, Source/ConvaiHTTPTransfer and Resources/Transfer/manifest.json in the same plugin root. Build the plugin in an HTTP-only bootstrap descriptor, then use the host's full cook descriptor. The host owns build/cook locking, other-plugin exclusion, cancellation and readiness verification.

## Invocation and files

Launch UnrealEditor-Cmd against an enabled-plugin project with `-run=ConvaiAvatarTransport -AvatarTransferRequest=<absolute private JSON file> -nullrhi -RenderOffscreen`. Put only the request file path in arguments: never a signed URL or credential. The host must protect that file and keep the original noninherited DELETE_ON_CLOSE handle alive with read sharing. The worker reads at most 64 KiB and suppresses the HTTP category before setting any URL.

The request contains `method` (GET or PUT), `url` (an allowed signed storage URL) and `file` (an absolute local source/destination path). Production URLs are limited to the existing user-assets-storage Google Storage bucket hosts. Local paths reject redirected ancestors; a GET destination must not already exist. The worker is an artifact transport, not an account or metadata client.

Results are written atomically beside the request as `.result.json`, with schema_version=1, success, a generic error, http_status, bytes (decimal string), md5, and transport=`CONVAIHTTP-transfer-v3`. `.progress.json` contains bytes and total as decimal strings. A GET uses an exclusive `.body.part` and installs only after successful completion and length validation. Bodies never accumulate in a response array. The exact cap is 10,485,760,000 bytes; a PUT response is capped at 1 MiB. URLs, credentials and response bodies are never copied into results/progress.

## Request-level transport security

`IConvaihttpRequest::SetTransportSecurityOptions` accepts `FConvaihttpTransportSecurityOptions` and returns whether the backend accepted the policy before processing. Curl supports it; adapters forward it. Backends without support return false instead of silently pretending enforcement.

Defaults are `bRequireVerifiedTls=false`, `bFollowRedirects=true`. Existing callers that never set this policy retain the constructor's global peer-verification setting and redirect behavior. Requiring verified TLS can strengthen that global setting; it cannot disable a global requirement. Hostname verification remains enabled. The commandlet explicitly requires verified TLS and forbids redirects, and fails before starting a request if those controls cannot be applied. It does not enable insecure TLS or change global curl settings.

## Verification

New offline native tests: `ConvaiHTTP.Streaming.DirectBufferAnd64BitOffsets` and `ConvaiHTTP.Streaming.ArchiveFailures`. They use a synthetic 5 GiB stream with tiny buffers, not large disk fixtures or a network connection, and cover direct output mutation, high offsets, a partial final chunk, EOF, rewind and archive failures.

The integration owner must build the candidate and rerun the existing actual commandlet loopback suite: GET/PUT checksums above 2 GiB and at the service cap, header casing and lengths, redirect refusal, invalid/untrusted TLS, truncated/oversized response bodies, and failure privacy. `-AvatarTransportTestLoopback` permits only numeric-port 127.0.0.1 HTTP/HTTPS URLs for that direct commandlet fixture; the normal host exposes no switch to enable it. Existing worker validation is evidence for the copied implementation, not a substitute for testing this module integration. No candidate build or live transfer is claimed by this document.
