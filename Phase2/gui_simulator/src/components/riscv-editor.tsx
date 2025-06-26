"use client"

import { useEffect, useRef } from "react"
import { Button } from "@/components/ui/button"
import { Card } from "@/components/ui/card"
import { Play } from "lucide-react"
import { loader } from "@monaco-editor/react"

// Dynamically import Monaco Editor to avoid SSR issues
import dynamic from "next/dynamic"
const MonacoEditor = dynamic(() => import("@monaco-editor/react"), { ssr: false })

interface RiscVEditorProps {
  value: string
  onChange: (value: string) => void
  onAssemble: () => void
}

export default function RiscVEditor({ value, onChange, onAssemble }: RiscVEditorProps) {
  const editorRef = useRef<any>(null)

  // Configure Monaco Editor
  useEffect(() => {
    // Define RISC-V syntax highlighting
    loader.init().then((monaco) => {
      monaco.languages.register({ id: "riscv" })
      monaco.languages.setMonarchTokensProvider("riscv", {
        tokenizer: {
          root: [
            // Comments
            [/#.*$/, "comment"],

            // Sections
            [/\.(text|data|globl|section)/, "keyword"],

            // Directives
            [/\.(word|byte|half|double|asciiz|ascii|align|space)/, "keyword"],

            // Labels
            [/[a-zA-Z0-9_]+:/, "type"],

            // Instructions
            [
              /\b(add|addi|sub|and|andi|or|ori|xor|sll|srl|sra|slt|mul|div|rem|lb|lh|lw|ld|sb|sh|sw|sd|beq|bne|bge|blt|jal|jalr|auipc|lui)\b/,
              "keyword",
            ],

            // Registers
            [/\b(zero|ra|sp|gp|tp|t[0-6]|s[0-9]|s10|s11|a[0-7]|x[0-9]|x1[0-9]|x2[0-9]|x3[0-1])\b/, "variable"],

            // Numbers
            [/\b[0-9]+\b/, "number"],
            [/\b0x[0-9a-fA-F]+\b/, "number"],

            // Strings
            [/".*?"/, "string"],

            // Characters
            [/'.'/, "string"],
          ],
        },
      })
    })
  }, [])

  useEffect(() => {
    // Workaround for ResizeObserver loop error
    const originalError = window.console.error
    window.console.error = (...args) => {
      if (args[0]?.includes?.("ResizeObserver loop")) {
        return
      }
      originalError(...args)
    }

    return () => {
      window.console.error = originalError
    }
  }, [])

  const handleEditorDidMount = (editor: any, monaco: any) => {
    editorRef.current = editor

    // Enable paste functionality
    editor.addCommand(monaco.KeyMod.CtrlCmd | monaco.KeyCode.KeyV, () => {
      navigator.clipboard
        .readText()
        .then((text) => {
          const selection = editor.getSelection()
          editor.executeEdits("clipboard", [
            {
              range: selection,
              text: text,
              forceMoveMarkers: true,
            },
          ])
        })
        .catch((err) => {
          console.error("Failed to read clipboard contents: ", err)
        })
    })
  }

  return (
    <Card className="flex flex-col h-full">
      <div className="p-2 border-b flex justify-between items-center">
        <h2 className="text-lg font-semibold">RISC-V Assembly Editor</h2>
        <Button onClick={onAssemble} className="flex items-center gap-2">
          <Play size={16} />
          Assemble
        </Button>
      </div>
      <div className="flex-1">
        <MonacoEditor
          height="100%"
          language="riscv"
          theme="vs" // Changed from vs-dark to vs (light theme)
          value={value}
          onChange={(value) => onChange(value || "")}
          onMount={handleEditorDidMount}
          options={{
            minimap: { enabled: true },
            scrollBeyondLastLine: false,
            fontSize: 14,
            tabSize: 4,
            automaticLayout: true,
            contextmenu: true, // Enable context menu for paste
            copyWithSyntaxHighlighting: true,
            // Enable all editor features
            quickSuggestions: true,
            snippetSuggestions: "inline",
            suggestOnTriggerCharacters: true,
          }}
        />
      </div>
    </Card>
  )
}

