public enum M640GKitDeactivatePatchResult {
    case success
    case failure(error: M640GKitDeactivatePatchError)
}

public enum M640GKitDeactivatePatchError: LocalizedError {
    case connectionFailure
    case unknownError(reason: String)
}
