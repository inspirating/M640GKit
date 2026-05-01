public enum M640GKitUpdatePatchResult {
    case success
    case failure(error: M640GKitUpdatePatchError)
}

public enum M640GKitUpdatePatchError: LocalizedError {
    case connectionFailure
    case unknownError(reason: String)
}
