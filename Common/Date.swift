let baseUnix: TimeInterval = .seconds(1_388_534_400) // 2014-01-01T00:00:00+0000

extension Date {
    static func fromM640GSeconds(_ seconds: UInt64) -> Date {
        Date(timeIntervalSince1970: baseUnix + Double(seconds))
    }

    func toM640GSeconds() -> Data {
        let data = UInt64(timeIntervalSince1970 - baseUnix)
        return data.toData(length: 4)
    }
}
