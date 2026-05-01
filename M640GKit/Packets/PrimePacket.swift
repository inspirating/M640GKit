struct PrimePacketResponse {}

class PrimePacket: M640GKitBasePacket, M640GKitBasePacketProtocol {
    typealias T = PrimePacketResponse

    let commandType: UInt8 = CommandType.PRIME
    let mimimumDataSize: Int = 0

    func getRequestBytes() -> Data {
        Data([])
    }

    func parseResponse() -> PrimePacketResponse {
        PrimePacketResponse()
    }
}
