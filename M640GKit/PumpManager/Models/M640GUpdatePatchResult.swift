public enum M640GUpdatePatchResult {
    case success
    case failure(error: M640GUpdatePatchError)
}

public enum M640GUpdatePatchError: LocalizedError {
    case connectionFailure
    case unknownError(reason: String)
}
