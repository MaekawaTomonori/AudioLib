# CLAUDE.md

This file provides guidance to Claude Code when working with this repository.

## Purpose

This repository contains a standalone C++ Audio Library designed for reuse across multiple projects.  
It provides functionality for loading, managing, and playing audio with a simple and stable public API.

---

# Core Design Philosophy

The following design principles must always be respected.

## DRY (Don't Repeat Yourself)

Avoid duplicated logic.  
Common behavior should be extracted into reusable components.

## KISS (Keep It Simple, Stupid)

Prefer simple, understandable implementations over complex abstractions.

Complexity should only be introduced when it clearly improves usability, performance, or maintainability.

## YAGNI (You Aren't Gonna Need It)

Do not implement speculative features.  
Only implement functionality that is actually required.

## OCP (Open/Closed Principle)

The public API is the contract with the library user.

Public interfaces must remain open for use but closed for modification.

Do not break existing public API behavior when extending functionality.

New features must be added without changing existing public interfaces.

Internal implementation may change freely as long as the public contract is preserved.

When in doubt, ask: does this change affect what the library user sees?

If yes, treat it as a breaking change and handle with care.

## Role-Based Private Functions

Private functions must be organized by role so that behavior can be understood at a glance.

Group private functions under comments that clearly describe their role.

Examples:

// --- Playback Control ---
// --- State Management ---
// --- Resource Lookup ---

A reader should be able to scan the private section and immediately understand what each group is responsible for without reading the implementation.

Avoid mixing unrelated operations in a single private function.

## SRP (Single Responsibility Principle)

Each class should have a single responsibility.

Examples:

SoundHandle  
Represents a playable instance of an audio resource.

AudioSystem  
Controls playback and manages audio devices.

Decoder  
Responsible only for decoding audio file formats.

## SoC (Separation of Concerns)

Responsibilities must be separated clearly.

Examples:

Audio playback control  
Audio decoding  
Resource management  
Audio device interaction

These must be separated into independent modules.

## Boy Scout Rule

Always leave the code cleaner than you found it.

## Defensive Programming

The library must behave safely even when misused.

Examples:

Loading a missing file must not crash the program.  
Invalid handles must fail safely.  
Incorrect parameters should be handled gracefully.

## Naming Matters

Names must clearly describe purpose.

Avoid vague names such as:

Manager  
Helper  
Utils  
Data

Prefer names like:

AudioDevice  
SoundInstance  
AudioDecoder  
SoundHandle

---

# Library Independence

The following rules apply:

Do not depend on rendering libraries.  

The audio system should work in any application context.

---

# Public API Design

The public API must remain:

Stable  
Minimal  
Easy to use

Users should only interact with a small number of classes.

Examples:

AudioSystem  
Sound  
SoundHandle

Internal implementation details must remain hidden.

Examples of internal systems:

Audio backend implementation  
Audio decoding logic  
Streaming logic  
Thread management

These must not be exposed to users.

---

# Resource Management

Audio resources should be managed efficiently.

Recommended structure:

AudioSystem  
Responsible for audio device initialization and playback control.

Sound  
Represents a loaded audio asset.

SoundHandle  
Represents an active playback instance.

Multiple SoundHandle instances may reference the same Sound.

This allows efficient reuse of audio data.

---

# Audio Playback Features

The library should support the following core features.

Load audio files

Play audio

Stop audio

Pause and resume playback

Looping

Volume control

Pitch control

Multiple simultaneous playback instances

Optional streaming for large audio files

---

# Error Handling

Errors must never crash the application.

All failures must be handled safely.

Examples:

File not found  
Unsupported format  
Audio device unavailable  
Invalid handle usage

Errors should return safe defaults or error results.

---

# Thread Safety

Audio playback often runs on a separate thread.

Thread safety must be considered when:

Starting playback  
Stopping playback  
Destroying audio resources

Shared data must be protected properly.

Avoid unnecessary locks in performance-critical code.

---

# Performance Considerations

Audio playback must remain low latency.

Avoid dynamic allocations in hot paths.

Audio mixing and playback should be efficient.

Heavy processing must not occur on the main thread.

Streaming large files should avoid loading entire files into memory.

---

# External Dependencies

External dependencies must be minimal.

Possible backend options include:

miniaudio  
SDL audio  
WASAPI  
CoreAudio  
ALSA

Backend implementations should be isolated behind an abstraction layer.

---

# Code Style

Follow modern C++ standards.

Prefer:

RAII  
Smart pointers  
Move semantics  
Const correctness

Avoid:

Global mutable state  
Excessive macros  
Unclear ownership

---

# Documentation

Public APIs must be documented clearly.

Documentation should explain:

What the function does  
Expected parameters  
Failure behavior

Internal systems should also contain comments explaining complex logic.

---

# Testing

Audio functionality should be testable.

Where possible:

Core logic should be independent from platform backends.

This allows unit testing of decoding, resource management, and playback logic.

---

# Final Rule

This library exists to provide a simple, reliable, reusable audio system.

When making changes, always prioritize:

Stability  
Clarity  
Maintainability  
Portability