SELECT errorCodeToName(toUInt32(-1));
SELECT errorCodeToName(-1);
SELECT errorCodeToName(950); /* gap in error codes */
SELECT errorCodeToName(0);
SELECT errorCodeToName(1);
SELECT errorCodeToName(1004);
