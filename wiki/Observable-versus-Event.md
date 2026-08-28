# Observable versus Event

Choose Observable when notification completion is part of the producer's operation:

```text
producer operation
  -> update state
  -> notify observers synchronously
  -> continue / return
```

Choose ESPressio Event when producer and consumer should be scheduled independently:

```text
producer -> dispatch event -> producer continues
                         |
                         +--> asynchronous consumer
```

## Good Observable use cases

Lifecycle notifications, immediate state-change callbacks, diagnostics hooks, and low-overhead one-to-many observation where callbacks are expected to complete promptly.

## Poor Observable use cases

Do not use Observable as a work queue, ISR-to-task transport, or substitute for asynchronous scheduling. Avoid unbounded/blocking observer work unless intentionally part of the producer operation.

## Bridging

A higher-level integration can synchronously observe a subsystem and translate selected notifications into Events. Observable itself remains independent of Event.