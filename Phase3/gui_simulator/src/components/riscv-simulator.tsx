"use client"

import type React from "react"

import { useState, useRef } from "react"
import { Button } from "@/components/ui/button"
import { Card, CardContent } from "@/components/ui/card"
import { Tabs, TabsContent, TabsList, TabsTrigger } from "@/components/ui/tabs"
import { Upload } from "lucide-react"
import RegisterDisplay from "./register-display"
import MemoryDisplay from "./memory-display"
import InstructionDisplay from "./instruction-display"
import { Simulator } from "@/lib/simulator"

export default function RiscVSimulator() {
  const [simulator, setSimulator] = useState<Simulator | null>(null)
  const [isFileLoaded, setIsFileLoaded] = useState(false)
  const [isRunning, setIsRunning] = useState(false)
  const [clockCycles, setClockCycles] = useState(0)
  const [currentPC, setCurrentPC] = useState(0)
  const [currentInstruction, setCurrentInstruction] = useState("")
  const [logs, setLogs] = useState<string[]>([])
  const fileInputRef = useRef<HTMLInputElement>(null)

  const handleFileUpload = (event: React.ChangeEvent<HTMLInputElement>) => {
    const file = event.target.files?.[0]
    if (!file) return

    const reader = new FileReader()
    reader.onload = (e) => {
      const content = e.target?.result as string
      const newSimulator = new Simulator()
      newSimulator.loadMcFile(content)
      setSimulator(newSimulator)
      setIsFileLoaded(true)
      setClockCycles(0)
      setCurrentPC(0)
      setCurrentInstruction("")
      setLogs([])
    }
    reader.readAsText(file)
  }

  const handleStep = () => {
    if (!simulator) return
    if (!simulator.hasNextInstruction()) {
      setLogs((prev) => [...prev, "Program execution completed"])
      return
    }

    const stepLogs: string[] = []
    simulator.step(stepLogs)
    setLogs((prev) => [...prev, ...stepLogs])
    setClockCycles(simulator.getClockCycles())
    setCurrentPC(simulator.getPC())
    setCurrentInstruction(simulator.getCurrentInstructionString())
  }

  const handleReset = () => {
    if (!simulator) return
    simulator.reset()
    setClockCycles(0)
    setCurrentPC(0)
    setCurrentInstruction("")
    setLogs([])
  }

  const handleRunAll = () => {
    if (!simulator) return
    setIsRunning(true)

    const runUntilComplete = () => {
      if (!simulator.hasNextInstruction) {
        setIsRunning(false)
        setLogs((prev) => [...prev, "Program execution completed"])
        return
      }

      const stepLogs: string[] = []
      simulator.step(stepLogs)
      setLogs((prev) => [...prev, ...stepLogs])
      setClockCycles(simulator.getClockCycles())
      setCurrentPC(simulator.getPC())
      setCurrentInstruction(simulator.getCurrentInstructionString())

      setTimeout(runUntilComplete, 100)
    }

    runUntilComplete()
  }

  const handleStop = () => {
    setIsRunning(false)
  }

  return (
    <div className="grid grid-cols-1 lg:grid-cols-3 gap-4">
      <div className="lg:col-span-2">
        <Card className="mb-4">
          <CardContent className="p-4">
            <div className="flex flex-col gap-4">
              <div className="flex items-center gap-2">
                <Button onClick={() => fileInputRef.current?.click()} className="flex items-center gap-2">
                  <Upload size={16} />
                  Upload .mc File
                </Button>
                <input type="file" ref={fileInputRef} onChange={handleFileUpload} accept=".mc" className="hidden" />
                <div className="text-sm">{isFileLoaded ? "File loaded successfully" : "No file loaded"}</div>
              </div>

              <div className="flex items-center gap-2">
                <Button onClick={handleStep} disabled={!isFileLoaded || isRunning}>
                  Step
                </Button>
                {!isRunning ? (
                  <Button onClick={handleRunAll} disabled={!isFileLoaded}>
                    Run All
                  </Button>
                ) : (
                  <Button onClick={handleStop} variant="destructive">
                    Running
                  </Button>
                )}
                <Button onClick={handleReset} disabled={!isFileLoaded || isRunning} variant="outline">
                  Reset
                </Button>
              </div>

              <div className="grid grid-cols-2 gap-4">
                <div className="text-sm">
                  <span className="font-bold">Clock Cycles:</span> {clockCycles}
                </div>
                <div className="text-sm">
                  <span className="font-bold">Current PC:</span> 0x
                  {currentPC.toString(16).padStart(8, "0").toUpperCase()}
                </div>
              </div>

              {currentInstruction && (
                <div className="text-sm">
                  <span className="font-bold">Current Instruction:</span> {currentInstruction}
                </div>
              )}
            </div>
          </CardContent>
        </Card>

        <Tabs defaultValue="instructions">
          <TabsList className="grid grid-cols-3 mb-4">
            <TabsTrigger value="instructions">Instructions</TabsTrigger>
            <TabsTrigger value="memory">Memory</TabsTrigger>
            <TabsTrigger value="logs">Execution Logs</TabsTrigger>
          </TabsList>

          <TabsContent value="instructions" className="h-[500px] overflow-auto">
            <Card>
              <CardContent className="p-4">
                <InstructionDisplay instructions={simulator?.getInstructions() || []} currentPC={currentPC} />
              </CardContent>
            </Card>
          </TabsContent>

          <TabsContent value="memory" className="h-[500px] overflow-auto">
            <Card>
              <CardContent className="p-4">
                <MemoryDisplay memory={simulator?.getMemory() || new Map()} />
              </CardContent>
            </Card>
          </TabsContent>

          <TabsContent value="logs" className="h-[500px] overflow-auto">
            <Card>
              <CardContent className="p-4">
                <div className="font-mono text-xs whitespace-pre-wrap">
                  {logs.map((log, index) => (
                    <div key={index} className="mb-1">
                      {log}
                    </div>
                  ))}
                </div>
              </CardContent>
            </Card>
          </TabsContent>
        </Tabs>
      </div>

      <div>
        <Card className="h-full">
          <CardContent className="p-4">
            <h2 className="text-lg font-bold mb-4">Registers</h2>
            <RegisterDisplay registers={simulator?.getRegisters() || new Array(32).fill(0)} />
          </CardContent>
        </Card>
      </div>
    </div>
  )
}

