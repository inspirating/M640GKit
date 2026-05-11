public enum M640GDeactivatePatchResult {
    case success
    case failure(error: M640GDeactivatePatchError)
}

public enum M640GDeactivatePatchError: LocalizedError {
    case connectionFailure
    case unknownError(reason: String)
}
