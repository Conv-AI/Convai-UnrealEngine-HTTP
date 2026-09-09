# ConvaiHTTP file transfer commandlet

The `ConvaiHTTPTransfer` module provides a Windows editor commandlet for large, streamed Cloud Avatars GET and PUT transfers. It uses only Core, CoreUObject, Engine, Json and CONVAIHTTP. The plugin descriptor loads it at Default in Editor targets on Win64; it is not a runtime game module.

This candidate contains main plus the five staging commits through `d40f5b1`, including response-body archive streaming. It moves the already exercised SDK worker into the HTTP plugin, fixes direct-buffer file reads, preserves 64-bit body lengths, accepts case-insensitive Content-Length, and uses curl's large upload-length option for PUT/PATCH. POST no longer replaces its large size with a long-sized option. No SDK-local source patch or game-module template is needed.

## Remote distribution contract

Distribute the complete plugin directory in the versioned `Convai-UnrealEngine-HTTP.zip` staging prerelease. Consumers resolve Convai's published remote dependency configuration, then verify the archive SHA256. `Resources/Transfer/manifest.json` declares the module and stable protocol without tying identity to a commit. Production main/latest release settings are separate from staging publication.

The archive contains ConvaiHTTP.uplugin, Source/CONVAIHTTP, Source/ConvaiHTTPTransfer and Resources/Transfer/manifest.json in the same plugin root. The UE5.8 Windows editor package also includes `UnrealEditor-CONVAIHTTP.dll`, `UnrealEditor-ConvaiHTTPTransfer.dll`, and `UnrealEditor.modules` under Binaries/Win64. Matching installed Unreal build IDs can load these binaries without a compiler. This is an editor/commandlet prebuilt, not a Shipping game binary distribution; source remains available for other builds and C++ consumers.

`Resources/Transfer/prebuilt.json` has schema_version=1, engine_version, engine_build_id, platform=Win64, configuration=Development, and a files object mapping those three exact plugin-relative paths to SHA256. Validate the manifest, hashes and module receipt before installation. An engine BuildId mismatch requires compatible binaries or a local source build; never load the mismatched DLLs. The verified package targets UE5.8.2, installed BuildId `55116800`.

For source fallback, temporarily use an HTTP-only descriptor in the same project and invoke `Build.bat UnrealEditor Win64 Development -Module=CONVAIHTTP -Module=ConvaiHTTPTransfer -Project=<project>`. UnrealEditor's target builds all modules unless these filters are supplied. Filtered builds omit module metadata; after a successful build and binary checks, write only this plugin's .modules receipt using the actual installed engine BuildId and the two fixed DLL names. Restore the full cook descriptor on completion or failure. The host owns build/cook locking, other-plugin exclusion, cancellation, SDK preservation and readiness verification.

## Invocation and files

Launch UnrealEditor-Cmd against an enabled-plugin project with `-run=ConvaiAvatarTransport -AvatarTransferRequest=<absolute private JSON file> -nullrhi -RenderOffscreen`. Put only the request file path in arguments: never a signed URL or credential. The host must protect that file and keep the original noninherited DELETE_ON_CLOSE handle alive with read sharing. The worker reads at most 64 KiB and suppresses the HTTP category before setting any URL.

The request contains `method` (GET or PUT), `url` (an allowed signed storage URL) and `file` (an absolute local source/destination path). Production URLs are limited to the existing user-assets-storage Google Storage bucket hosts. Local paths reject redirected ancestors; a GET destination must not already exist. The worker is an artifact transport, not an account or metadata client.

Results are written atomically beside the request as `.result.json`, with schema_version=1, success, a generic error, http_status, bytes (decimal string), md5, and transport=`CONVAIHTTP-transfer-v3`. `.progress.json` contains bytes and total as decimal strings. A GET uses an exclusive `.body.part` and installs only after successful completion and length validation. Bodies never accumulate in a response array. The exact cap is 10,485,760,000 bytes; a PUT response is capped at 1 MiB. URLs, credentials and response bodies are never copied into results/progress.

## Request-level transport security

`IConvaihttpRequest::SetTransportSecurityOptions` accepts `FConvaihttpTransportSecurityOptions` and returns whether the backend accepted the policy before processing. Curl supports it; adapters forward it. Backends without support return false instead of silently pretending enforcement.

Defaults are `bRequireVerifiedTls=false`, `bFollowRedirects=true`. Existing callers that never set this policy retain the constructor's global peer-verification setting and redirect behavior. Requiring verified TLS can strengthen that global setting; it cannot disable a global requirement. Hostname verification remains enabled. The commandlet explicitly requires verified TLS and forbids redirects, and fails before starting a request if those controls cannot be applied. It does not enable insecure TLS or change global curl settings.

## Verification

New offline native tests: `ConvaiHTTP.Streaming.DirectBufferAnd64BitOffsets` and `ConvaiHTTP.Streaming.ArchiveFailures`. They use a synthetic 5 GiB stream with tiny buffers, not large disk fixtures or a network connection, and cover direct output mutation, high offsets, a partial final chunk, EOF, rewind and archive failures.

The UE5.8.2 integration build and both native tests passed. Actual commandlet PUT/GET of 3,221,225,595 bytes matched independent SHA256 and MD5. Redirect, denied PUT, truncated/oversized body, untrusted TLS and verified public TLS failure checks passed. Worker logs mounted no SDK or avatar plugins. A fresh prebuilt installation succeeded with no compiler available, created no build log/intermediates, preserved its project descriptor, and then passed both native tests through the actual installed Unreal editor.

The earlier worker implementation also passed a 10,485,760,000-byte roundtrip; that is historical transport evidence, not a claim that this new module integration has been rerun at the cap. `-AvatarTransportTestLoopback` permits only numeric-port 127.0.0.1 HTTP/HTTPS URLs for the direct fixture; the normal host exposes no switch to enable it. No production avatars or Linux execution were part of these checks.
