Google Ads Confidential Computing - Code Samples

# Projects

This repository contains sample code demonstrating how to use a [Trusted Execution Environment (TEE) application](https://github.com/google-ads-confidential-computing/conf-data-processing-architecture-reference/blob/main/docs/TrustedExecutionEnvironmentsArchitecturalReference.md) to securely join publisher and advertiser data without either party discerning sensitive information from the other by following these steps:

- Publishers provide a list of their known PII (e.g., email addresses) to use for matching in a cloud storage location only accessible to them and the TEE application
- Advertisers provide a list of their corresponding PII in a separate cloud storage location only accessible to them and the TEE application
- Publishers provide a list of publisher identifiers (pub IDs) which map to their PII
- Advertisers request matches, the TEE application compares the two lists, and outputs a list of the matching surrogate identifiers in a cloud storage location accessible to them.
- Advertisers can then use these pub IDs downstream for example in audience targeting lists without directly knowing anything about the actual target

# Running

For instructions on building and running the TEE application, see [PAIRDEMO.md](PAIRDEMO.md).

# Contribution

Please see [CONTRIBUTING.md](CONTRIBUTING.md) for details.