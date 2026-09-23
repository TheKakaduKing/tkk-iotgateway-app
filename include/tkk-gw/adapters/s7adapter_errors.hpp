#pragma once

enum class AdapterError
{
};

enum class ConfigError
{
    FileNotFound,
    ParseError,
    MissingKey,
    MissingNode,
    TypeMismatch,
    DuplicateID,
    OutOfBound,
    IpMismatch,
    ItemNotFitPdu,

};

enum class s7Error
{
    ConnectionFailed,
    ReadFailed,
    PduError,
    PlcError,

};
static inline std::string ConfigErrorToString(ConfigError err_)
{
    switch (err_)
    {
    case ConfigError::FileNotFound:
    {
        return "File not found";
        break;
    }
    case ConfigError::ParseError:
    {
        return "Parse error";
        break;
    }
    case ConfigError::MissingKey:
    {
        return "Missing key";
        break;
    }
    case ConfigError::MissingNode:
    {
        return "Missing node";
        break;
    }
    case ConfigError::TypeMismatch:
    {
        return "Type mismatch";
        break;
    }
    case ConfigError::DuplicateID:
    {
        return "Duplicate ID";
        break;
    }
    case ConfigError::OutOfBound:
    {
        return "Out of bound";
        break;
    }
    case ConfigError::IpMismatch:
    {
        return "IP mismatch";
        break;
    }
    case ConfigError::ItemNotFitPdu:
    {
        return "Item does not fit Pdu";
        break;
    }

    default:
    {
        return "N/A";
        break;
    }
    }
}