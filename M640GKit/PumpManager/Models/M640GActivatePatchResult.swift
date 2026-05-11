public enum M640GActivatePatchResult {
    case success
    case failure(error: M640GActivatePatchError)
}

public enum M640GActivatePatchError: LocalizedError {
    case connectionFailure(reason: String)
    case unknownError(reason: String)

    public var errorDescription: String? {
        switch self {
        case let .connectionFailure(reason: reason):
            return "Connection failure: \(reason)"
        case let .unknownError(reason: reason):
            return "Unknown error: \(reason)"
        }
    }
}
